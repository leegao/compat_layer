#include "layer.hpp"

#include "logger.hpp"
#include "pipeline_state.hpp"
#include "vk_func.hpp"
#include "vulkan/vk_layer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unistd.h>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

std::unordered_map<void *, VkLayerInstanceDispatchTable> instanceDispatch;
std::unordered_map<void *, VkInstance> instanceMap;
std::unordered_map<void *, VkPhysicalDeviceFeatures> featuresMap;
std::unordered_map<void *, VkPhysicalDeviceProperties2> propertiesMap;
std::unordered_map<void *, VkPhysicalDeviceDriverProperties>
    driverPropertiesMap;
std::unordered_map<void *, std::shared_ptr<struct device>> deviceMap;

std::mutex global_lock;

static void *GetInstanceKey(VkPhysicalDevice physicalDevice) {
    auto instance = instanceMap[GetKey(physicalDevice)];
    if (!instance) {
        Logger::log("error", "no instance found for physical device %p",
                    GetKey(physicalDevice));
        return nullptr;
    }
    return GetKey(instance);
}

#define GETPROCADDR(func)                                                      \
    if (!strcmp(pName, "vk" #func))                                            \
        return (PFN_vkVoidFunction) & DxvkMaliCompatLayer_##func;

struct device *get_device(VkDevice device) {
    auto it = deviceMap.find(GetKey(device));

    if (it == deviceMap.end())
        return nullptr;

    return it->second.get();
}

SyncPool::~SyncPool() {
    auto dev = get_device(device);
    if (!dev)
        return;
    for (auto f : freeFences)
        dev->table.DestroyFence(device, f, nullptr);
    for (auto s : freeSemaphores)
        dev->table.DestroySemaphore(device, s, nullptr);
}

std::pair<VkSemaphore, VkFence> SyncPool::Acquire() {
    auto dev = get_device(device);
    if (!dev)
        return {VK_NULL_HANDLE, VK_NULL_HANDLE};

    VkFence fence;
    if (!freeFences.empty()) {
        fence = freeFences.back();
        freeFences.pop_back();
        dev->table.ResetFences(device, 1, &fence);
    } else {
        VkFenceCreateInfo info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        dev->table.CreateFence(device, &info, nullptr, &fence);
    }

    VkSemaphore sem;
    if (!freeSemaphores.empty()) {
        sem = freeSemaphores.back();
        freeSemaphores.pop_back();
    } else {
        VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        dev->table.CreateSemaphore(device, &info, nullptr, &sem);
    }
    return {sem, fence};
}

void DescriptorSetAllocator::cleanup() {
    scoped_lock l(lock);
    if (activePool != VK_NULL_HANDLE) {
        device->table.DestroyDescriptorPool(device->handle, activePool,
                                            nullptr);
        activePool = VK_NULL_HANDLE;
    }
    for (auto pool : exhaustedPools) {
        device->table.DestroyDescriptorPool(device->handle, pool, nullptr);
    }
    exhaustedPools.clear();
    occupancy.clear();
}

VkResult DescriptorSetAllocator::allocate(VkDescriptorSetLayout layout,
                                          VkDescriptorPool *pool,
                                          VkDescriptorSet *descriptors) {
    // Both the Free/AllocateDescriptorSets and access to active/exhausted pools
    // must be synchronized
    scoped_lock l(lock);
    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = activePool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkResult result = VK_ERROR_OUT_OF_POOL_MEMORY;
    if (activePool != VK_NULL_HANDLE) {
        result = device->table.AllocateDescriptorSets(device->handle,
                                                      &alloc_info, descriptors);
    }

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY ||
        result == VK_ERROR_FRAGMENTED_POOL || activePool == VK_NULL_HANDLE) {

        if (activePool != VK_NULL_HANDLE) {
            exhaustedPools.push_back(activePool);
            activePool = VK_NULL_HANDLE;
        }

        // Find a completely vacant pool
        auto it = std::find_if(
            exhaustedPools.begin(), exhaustedPools.end(),
            [this](VkDescriptorPool p) { return occupancy[p] == 0; });

        if (it != exhaustedPools.end()) {
            activePool = *it;
            exhaustedPools.erase(it);
            device->table.ResetDescriptorPool(device->handle, activePool, 0);
        } else {
            result = createNewPool(&activePool);
            if (result != VK_SUCCESS) {
                return result;
            }
        }

        alloc_info.descriptorPool = activePool;
        result = device->table.AllocateDescriptorSets(device->handle,
                                                      &alloc_info, descriptors);
    }

    if (result == VK_SUCCESS) {
        occupancy[activePool]++;
        allocated_count++;
        *pool = alloc_info.descriptorPool;
    }
    return result;
}

void DescriptorSetAllocator::free(VkDescriptorPool pool,
                                  VkDescriptorSet descriptors) {
    scoped_lock l(lock);
    device->table.FreeDescriptorSets(device->handle, pool, 1, &descriptors);
    allocated_count--;

    if (occupancy.find(pool) != occupancy.end() && occupancy[pool] > 0) {
        occupancy[pool]--;
    }

    if (pool != activePool && occupancy[pool] == 0) {
        auto vacantPools =
            std::count_if(exhaustedPools.begin(), exhaustedPools.end(),
                          [&](auto p) { return occupancy.at(p) <= 0; });

        // Destroy the pool if we have too many empty pools already
        if (vacantPools > maxEmptyPoolsToReserve) {
            auto it =
                std::find(exhaustedPools.begin(), exhaustedPools.end(), pool);
            if (it != exhaustedPools.end()) {
                exhaustedPools.erase(it);
            }
            device->table.DestroyDescriptorPool(device->handle, pool, nullptr);
            occupancy.erase(pool);
        }
    }
}

VkResult
DescriptorSetAllocator::createNewPool(VkDescriptorPool *descriptor_pool) {
    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = poolSizes.maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.sizes.size()),
        .pPoolSizes = poolSizes.sizes.data(),
    };

    auto result = device->table.CreateDescriptorPool(device->handle, &pool_info,
                                                     nullptr, descriptor_pool);
    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to allocate new descriptor pool: %d",
                    result);
        return result;
    }
    occupancy[*descriptor_pool] = 0;
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateInstance(
    const VkInstanceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkInstance *pInstance) {
    VkLayerInstanceCreateInfo *layerCreateInfo =
        (VkLayerInstanceCreateInfo *)pCreateInfo->pNext;
    VkResult result;

    while (layerCreateInfo &&
           (layerCreateInfo->sType !=
                VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
            layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
        layerCreateInfo = (VkLayerInstanceCreateInfo *)layerCreateInfo->pNext;
    }

    if (!layerCreateInfo)
        return VK_ERROR_INITIALIZATION_FAILED;

    PFN_vkGetInstanceProcAddr gip =
        layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateInstance createInstance =
        (PFN_vkCreateInstance)gip(VK_NULL_HANDLE, "vkCreateInstance");
    result = createInstance(pCreateInfo, pAllocator, pInstance);

    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to create instance, res %d", result);
        return result;
    }

    VkLayerInstanceDispatchTable table;
    table.GetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)gip(*pInstance, "vkGetInstanceProcAddr");
    table.DestroyInstance =
        (PFN_vkDestroyInstance)gip(*pInstance, "vkDestroyInstance");
    table.EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)gip(
        *pInstance, "vkEnumeratePhysicalDevices");
    table.GetPhysicalDeviceMemoryProperties =
        (PFN_vkGetPhysicalDeviceMemoryProperties)gip(
            *pInstance, "vkGetPhysicalDeviceMemoryProperties");
    table.GetPhysicalDeviceFormatProperties =
        (PFN_vkGetPhysicalDeviceFormatProperties)gip(
            *pInstance, "vkGetPhysicalDeviceFormatProperties");
    table.GetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)gip(
        *pInstance, "vkGetPhysicalDeviceProperties");
    table.GetPhysicalDeviceProperties2 =
        (PFN_vkGetPhysicalDeviceProperties2)gip(
            *pInstance, "vkGetPhysicalDeviceProperties2");
    if (!table.GetPhysicalDeviceProperties2) {
        table.GetPhysicalDeviceProperties2 =
            (PFN_vkGetPhysicalDeviceProperties2)gip(
                *pInstance, "vkGetPhysicalDeviceProperties2KHR");
    }
    table.GetPhysicalDeviceImageFormatProperties =
        (PFN_vkGetPhysicalDeviceImageFormatProperties)gip(
            *pInstance, "vkGetPhysicalDeviceImageFormatProperties");
    table.GetPhysicalDeviceImageFormatProperties2 =
        (PFN_vkGetPhysicalDeviceImageFormatProperties2)gip(
            *pInstance, "vkGetPhysicalDeviceImageFormatProperties2");
    table.GetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)gip(
        *pInstance, "vkGetPhysicalDeviceFeatures");
    table.GetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2)gip(
        *pInstance, "vkGetPhysicalDeviceFeatures2");
    if (!table.GetPhysicalDeviceFeatures2) {
        table.GetPhysicalDeviceFeatures2 =
            (PFN_vkGetPhysicalDeviceFeatures2)gip(
                *pInstance, "vkGetPhysicalDeviceFeatures2KHR");
    }
    table.GetPhysicalDeviceQueueFamilyProperties =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties)gip(
            *pInstance, "vkGetPhysicalDeviceQueueFamilyProperties");
    table.EnumerateDeviceExtensionProperties =
        (PFN_vkEnumerateDeviceExtensionProperties)gip(
            *pInstance, "vkEnumerateDeviceExtensionProperties");

    {
        scoped_lock l(global_lock);
        instanceDispatch[GetKey(*pInstance)] = table;
        instanceMap[GetKey(*pInstance)] = *pInstance;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyInstance(
    VkInstance instance, const VkAllocationCallbacks *pAllocator) {
    scoped_lock l(global_lock);
    if (!instance)
        return;

    VkLayerInstanceDispatchTable table = instanceDispatch[GetKey(instance)];
    table.DestroyInstance(instance, pAllocator);
    instanceMap.erase(GetKey(instance));
    instanceDispatch.erase(GetKey(instance));
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DxvkMaliCompatLayer_EnumeratePhysicalDevices(
    VkInstance instance, uint32_t *pPhysicalDeviceCount,
    VkPhysicalDevice *pPhysicalDevices) {
    scoped_lock l(global_lock);

    VkResult result;

    result = instanceDispatch[GetKey(instance)].EnumeratePhysicalDevices(
        instance, pPhysicalDeviceCount, pPhysicalDevices);

    if (result != VK_SUCCESS || *pPhysicalDeviceCount < 1 ||
        pPhysicalDevices == nullptr)
        return result;

    for (uint32_t index = 0; index < *pPhysicalDeviceCount; index++) {
        VkPhysicalDeviceFeatures features{};
        instanceDispatch[GetKey(instance)].GetPhysicalDeviceFeatures(
            pPhysicalDevices[index], &features);

        VkPhysicalDeviceDriverProperties driverProperties{};
        VkPhysicalDeviceProperties2 props2{};
        driverProperties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &driverProperties;
        instanceDispatch[GetKey(instance)].GetPhysicalDeviceProperties2(
            pPhysicalDevices[index], &props2);

        featuresMap[GetKey(pPhysicalDevices[index])] = features;
        propertiesMap[GetKey(pPhysicalDevices[index])] = props2;
        driverPropertiesMap[GetKey(pPhysicalDevices[index])] = driverProperties;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_GetPhysicalDeviceFeatures(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures *pFeatures) {
    scoped_lock l(global_lock);

    instanceDispatch[GetInstanceKey(physicalDevice)].GetPhysicalDeviceFeatures(
        physicalDevice, pFeatures);
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_GetPhysicalDeviceFeatures2(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures) {
    scoped_lock l(global_lock);

    instanceDispatch[GetInstanceKey(physicalDevice)].GetPhysicalDeviceFeatures2(
        physicalDevice, pFeatures);

    for (auto *next = static_cast<VkBaseOutStructure *>(pFeatures->pNext);
         next != nullptr; next = next->pNext) {
        if (next->sType ==
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT) {
            auto *features =
                reinterpret_cast<VkPhysicalDeviceDescriptorBufferFeaturesEXT *>(
                    next);
            features->descriptorBuffer = VK_TRUE;
            features->descriptorBufferCaptureReplay = VK_FALSE; // not emulated
            features->descriptorBufferImageLayoutIgnored =
                VK_FALSE; // not emulated
            // TODO(leegao): needed for the optimal_performance profile in vkd3d
            // https://github.com/HansKristian-Work/vkd3d-proton/blob/ce3c862c26ef5b1b74a3a24cf637bddb1adb7a7c/VP_D3D12_VKD3D_PROTON_profile.json#L502
            features->descriptorBufferPushDescriptors =
                VK_FALSE; // not emulated
        }
    }
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_GetPhysicalDeviceProperties2(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2 *pProperties) {
    scoped_lock l(global_lock);

    instanceDispatch[GetInstanceKey(physicalDevice)]
        .GetPhysicalDeviceProperties2(physicalDevice, pProperties);

    for (auto *next = static_cast<VkBaseOutStructure *>(pProperties->pNext);
         next != nullptr; next = next->pNext) {
        if (next->sType ==
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT) {
            auto *props = reinterpret_cast<
                VkPhysicalDeviceDescriptorBufferPropertiesEXT *>(next);
            props->combinedImageSamplerDescriptorSingleArray = VK_TRUE;
            props->bufferlessPushDescriptors = VK_FALSE;
            props->allowSamplerImageViewPostSubmitCreation = VK_FALSE;
            // TODO ...
        }
    }
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DxvkMaliCompatLayer_EnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice, const char *pLayerName,
    uint32_t *pPropertyCount, VkExtensionProperties *pProperties) {
    scoped_lock l(global_lock);

    auto &table = instanceDispatch[GetInstanceKey(physicalDevice)];

    if (pProperties == nullptr) {
        VkResult result = table.EnumerateDeviceExtensionProperties(
            physicalDevice, pLayerName, pPropertyCount, nullptr);
        if (result == VK_SUCCESS)
            *pPropertyCount += 1;
        return result;
    }

    uint32_t capacity = *pPropertyCount;
    if (capacity == 0) {
        return VK_SUCCESS;
    }

    uint32_t queryCount = capacity - 1;
    VkResult result = table.EnumerateDeviceExtensionProperties(
        physicalDevice, pLayerName, &queryCount, pProperties);

    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        return result;
    }

    VkExtensionProperties vkExtDescriptorBuffer{
        .extensionName = VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
        .specVersion = VK_EXT_DESCRIPTOR_BUFFER_SPEC_VERSION,
    };

    pProperties[queryCount] = vkExtDescriptorBuffer;
    *pPropertyCount = queryCount + 1;

    uint32_t count = 0;
    table.EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, &count,
                                             nullptr);

    if ((*pPropertyCount) < (count + 1)) {
        return VK_INCOMPLETE;
    }

    return VK_SUCCESS;
}

void FinalizerThread(struct device *dev) {
    while (!dev->stop_thread) {
        {
            std::vector<std::unique_ptr<StagingResources>> queue;
            {
                std::unique_lock<std::mutex> lock(global_lock);
                dev->hasCleanupWork.wait(lock, [dev] {
                    return dev->stop_thread ||
                           !dev->stagingResourcesQueue.empty();
                });

                if (dev->stop_thread) {
                    return;
                }

                if (!dev->stagingResourcesQueue.empty()) {
                    std::swap(queue, dev->stagingResourcesQueue);
                }
            }

            while (!queue.empty()) {
                if (dev->stop_thread)
                    return;
                std::unique_ptr<StagingResources> stagingResources =
                    std::move(queue.back());
                queue.pop_back();
                if (stagingResources) {
                    stagingResources->WaitForCompletion();
                    stagingResources->Cleanup();
                }
            }
        }
    }
}

struct BufferFeatures {
    bool shaderBufferInt64Atomics;
    bool shaderSharedInt64Atomics;
    bool storageBuffer8BitAccess;
    bool storageBuffer16BitAccess;
    bool shaderInt8;
    bool shaderFloat16;
};

BufferFeatures CheckForBufferFeatureSupport(VkInstance instance,
                                            VkPhysicalDevice physicalDevice) {
    auto props = propertiesMap[GetKey(physicalDevice)];
    if (props.properties.apiVersion < VK_API_VERSION_1_1) {
        // 1.0 does not support 8bit
        return {
            .shaderBufferInt64Atomics = false,
            .shaderSharedInt64Atomics = false,
            .storageBuffer8BitAccess = false,
            .storageBuffer16BitAccess = false,
            .shaderInt8 = false,
            .shaderFloat16 = false,
        };
    }

    VkPhysicalDeviceShaderAtomicInt64Features int64Atomics = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES,
    };

    VkPhysicalDevice8BitStorageFeaturesKHR storage8Bit = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,
        .pNext = &int64Atomics};

    VkPhysicalDevice16BitStorageFeaturesKHR storage16Bit = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR,
        .pNext = &storage8Bit};

    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR arithmeticFloat16Int8 = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
        .pNext = &storage16Bit,
    };

    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &arithmeticFloat16Int8,
    };

    instanceDispatch[GetKey(instance)].GetPhysicalDeviceFeatures2(
        physicalDevice, &features2);
    return {
        .shaderBufferInt64Atomics =
            (int64Atomics.shaderBufferInt64Atomics == VK_TRUE),
        .shaderSharedInt64Atomics =
            (int64Atomics.shaderSharedInt64Atomics == VK_TRUE),
        .storageBuffer8BitAccess =
            (storage8Bit.storageBuffer8BitAccess == VK_TRUE),
        .storageBuffer16BitAccess =
            (storage16Bit.storageBuffer16BitAccess == VK_TRUE),
        .shaderInt8 = (arithmeticFloat16Int8.shaderInt8 == VK_TRUE),
        .shaderFloat16 = (arithmeticFloat16Int8.shaderFloat16 == VK_TRUE),
    };
}

VkPhysicalDeviceFaultFeaturesEXT
CheckForFaultSupport(VkInstance instance, VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceFaultFeaturesEXT faultFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT,
    };
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &faultFeatures,
    };

    instanceDispatch[GetKey(instance)].GetPhysicalDeviceFeatures2(
        physicalDevice, &features);

    return faultFeatures;
}

VkPhysicalDeviceBufferDeviceAddressFeatures
CheckForBufferDeviceAddressSupport(VkInstance instance,
                                   VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
    };
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &bdaFeatures,
    };

    instanceDispatch[GetKey(instance)].GetPhysicalDeviceFeatures2(
        physicalDevice, &features);

    return bdaFeatures;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateDevice(
    VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) {
    VkResult result;
    VkLayerDeviceCreateInfo *layerCreateInfo =
        (VkLayerDeviceCreateInfo *)pCreateInfo->pNext;
    VkDeviceCreateInfo createInfo = *pCreateInfo;

    while (layerCreateInfo &&
           (layerCreateInfo->sType !=
                VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
            layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
        layerCreateInfo = (VkLayerDeviceCreateInfo *)layerCreateInfo->pNext;
    }

    if (layerCreateInfo == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr gipa =
        layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa =
        layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    bool has_more_layers = layerCreateInfo->u.pLayerInfo->pNext != nullptr;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    VkInstance instance = instanceMap[GetKey(physicalDevice)];
    if (instance == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkPhysicalDeviceMemoryProperties memoryProps{};
    uint32_t idx;

    instanceDispatch[GetKey(instance)].GetPhysicalDeviceMemoryProperties(
        physicalDevice, &memoryProps);
    for (idx = 0; idx < memoryProps.memoryTypeCount; idx++) {
        if (memoryProps.memoryTypes[idx].propertyFlags &
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            break;
    }

    auto profile_transfers = getenv("COMPAT_PROFILE_TRANSFERS")
                                 ? atoi(getenv("COMPAT_PROFILE_TRANSFERS"))
                                 : 0;
    auto sample_gpu_counters = getenv("COMPAT_SAMPLE_GPU_COUNTERS")
                                   ? atoi(getenv("COMPAT_SAMPLE_GPU_COUNTERS"))
                                   : 0;

    auto memoryIndex = idx < memoryProps.memoryTypeCount ? idx : UINT32_MAX;
    auto queriedFaultFeatures = CheckForFaultSupport(instance, physicalDevice);
    bool hasFaultSupport = queriedFaultFeatures.deviceFault;
    auto queriedBDAFeatures =
        CheckForBufferDeviceAddressSupport(instance, physicalDevice);

    if (!queriedBDAFeatures.bufferDeviceAddress) {
        Logger::log("error", "FATAL: " VK_EXT_DEVICE_FAULT_EXTENSION_NAME
                             " not supported, cannot proceed.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkBaseOutStructure *ext = (VkBaseOutStructure *)createInfo.pNext;
    VkPhysicalDeviceFeatures2 *appFeatures2 = nullptr;
    VkPhysicalDeviceFaultFeaturesEXT *appFaultFeatures = nullptr;

    while (ext) {
        if (ext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
            appFeatures2 = (VkPhysicalDeviceFeatures2 *)ext;
        } else if (ext->sType ==
                   VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT) {
            appFaultFeatures = (VkPhysicalDeviceFaultFeaturesEXT *)ext;
        }
        ext = ext->pNext;
    }

    VkPhysicalDeviceProperties2 physProps =
        propertiesMap[GetKey(physicalDevice)];
    uint32_t apiVersion = physProps.properties.apiVersion;

    std::vector<const char *> enabledExtensions;
    if (createInfo.ppEnabledExtensionNames &&
        createInfo.enabledExtensionCount > 0) {
        for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i) {
            if (strcmp(createInfo.ppEnabledExtensionNames[i],
                       VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME) != 0) {
                enabledExtensions.push_back(
                    createInfo.ppEnabledExtensionNames[i]);
            }
        }
    }

    auto hasExtension = [&](const char *name) {
        for (const auto &extName : enabledExtensions) {
            if (strcmp(extName, name) == 0)
                return true;
        }
        return false;
    };

    if (hasFaultSupport && !hasExtension(VK_EXT_DEVICE_FAULT_EXTENSION_NAME)) {
        Logger::log("info",
                    "Adding extension " VK_EXT_DEVICE_FAULT_EXTENSION_NAME);
        enabledExtensions.push_back(VK_EXT_DEVICE_FAULT_EXTENSION_NAME);
    }

    if (!hasExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
        Logger::log(
            "info",
            "Adding extension " VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        enabledExtensions.push_back(
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    }

    createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(enabledExtensions.size());

    VkPhysicalDeviceFaultFeaturesEXT layerFaultFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT,
        .deviceFault = VK_TRUE,
        .deviceFaultVendorBinary = queriedFaultFeatures.deviceFaultVendorBinary,
    };

    if (hasFaultSupport) {
        if (appFaultFeatures) {
            appFaultFeatures->deviceFault = VK_TRUE;
            appFaultFeatures->deviceFaultVendorBinary =
                queriedFaultFeatures.deviceFaultVendorBinary;
        } else {
            Logger::log("info", "Enabling VK_EXT_device_fault features");
            layerFaultFeatures.pNext = (void *)createInfo.pNext;
            createInfo.pNext = &layerFaultFeatures;
        }
    }

    PFN_vkCreateDevice createDevice =
        (PFN_vkCreateDevice)gipa(instance, "vkCreateDevice");
    result = createDevice(physicalDevice, &createInfo, pAllocator, pDevice);

    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to create device, res %d", result);
        return result;
    }

    VkLayerDispatchTable table;
    init_dispatch_table(gdpa, *pDevice, table);

    if (table.DeviceSetApiDumpState) {
        Logger::log("info", "[DEBUG] vkDeviceSetApiDumpState "
                            "(VK_LAYER_LUNARG_api_dump) is enabled");
    }

    uint32_t queueCount;
    VkQueue queue;

    std::vector<VkQueueFamilyProperties> queueProps;
    instanceDispatch[GetKey(instance)].GetPhysicalDeviceQueueFamilyProperties(
        physicalDevice, &queueCount, nullptr);
    queueProps.resize(queueCount);
    instanceDispatch[GetKey(instance)].GetPhysicalDeviceQueueFamilyProperties(
        physicalDevice, &queueCount, queueProps.data());

    uint32_t i = 0;
    for (const auto &family : queueProps) {
        if (family.queueFlags & VK_QUEUE_COMPUTE_BIT)
            break;
        i++;
    }

    table.GetDeviceQueue(*pDevice, i, 0, &queue);

    auto device = std::make_shared<struct device>();
    device->handle = *pDevice;
    device->physical = physicalDevice;
    device->props2 = propertiesMap[GetKey(physicalDevice)];
    device->driverProps = driverPropertiesMap[GetKey(physicalDevice)];
    device->features = featuresMap[GetKey(physicalDevice)];
    device->table = table;
    device->memoryIndex = memoryIndex;
    device->queue = queue;
    device->queueFamilyIndex = i;
    device->alloc = pAllocator;
    device->profile_transfers = profile_transfers;
    device->sample_gpu_counters = sample_gpu_counters;
    device->has_more_layers = has_more_layers;

    device->syncPool = std::make_unique<SyncPool>(device->handle);

    const DescriptorSetAllocator::PoolSizes default_pool_sizes{
        .sizes =
            {
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 .descriptorCount = 256},
                {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .descriptorCount = 256},
                {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 .descriptorCount = 256},
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                 .descriptorCount = 256},
                {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 256},
                {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 .descriptorCount = 256},
                {.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                 .descriptorCount = 256},
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                 .descriptorCount = 256},
            },
        .maxSets = 256,
    };
    device->descriptorSetAllocator = std::make_unique<DescriptorSetAllocator>(
        device.get(), default_pool_sizes);

    device->stop_thread = false;
    device->finalizer_thread = std::thread(FinalizerThread, device.get());

    static std::atomic<uint64_t> nextDeviceIdCounter{1};
    device->deviceId = nextDeviceIdCounter.fetch_add(1);

    {
        scoped_lock l(global_lock);
        deviceMap[GetKey(*pDevice)] = device;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyDevice(
    VkDevice device, const VkAllocationCallbacks *pAllocator) {
    struct device *dev;
    {
        scoped_lock l(global_lock);
        dev = get_device(device);
        if (!dev)
            return;
    }

    dev->stop_thread = true;
    dev->hasCleanupWork.notify_all();
    dev->table.DeviceWaitIdle(device);
    if (dev->finalizer_thread.joinable()) {
        dev->finalizer_thread.join();
    }

    scoped_lock l(global_lock);
    for (auto &stagingResources : dev->stagingResourcesQueue) {
        stagingResources->Cleanup();
    }
    dev->stagingResourcesQueue.clear();
    dev->syncPool.reset();
    dev->descriptorSetAllocator->cleanup();
    dev->descriptorSetAllocator.reset();

    if (device != VK_NULL_HANDLE)
        dev->table.DestroyDevice(device, pAllocator);

    deviceMap.erase(GetKey(device));
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL
DxvkMaliCompatLayer_GetDeviceProcAddr(VkDevice device, const char *pName) {
    GETPROCADDR(DestroyDevice);
    // GETPROCADDR(CreateBuffer);
    // GETPROCADDR(BindBufferMemory);
    // GETPROCADDR(BindBufferMemory2);
    // GETPROCADDR(DestroyBuffer);
    GETPROCADDR(AllocateCommandBuffers);
    GETPROCADDR(FreeCommandBuffers);
    // GETPROCADDR(BeginCommandBuffer);
    // GETPROCADDR(ResetCommandBuffer);
    // GETPROCADDR(CmdBindPipeline);
    // GETPROCADDR(CmdBindDescriptorSets);
    // GETPROCADDR(CmdBindDescriptorSets2);
    // GETPROCADDR(CmdPushConstants);
    // GETPROCADDR(CmdPushConstants2);
    // GETPROCADDR(CreateDescriptorSetLayout);
    // GETPROCADDR(DestroyDescriptorSetLayout);
    // GETPROCADDR(CreatePipelineLayout);
    // GETPROCADDR(DestroyPipelineLayout);
    // GETPROCADDR(CreateGraphicsPipelines);
    // GETPROCADDR(CreateComputePipelines);
    // GETPROCADDR(DestroyPipeline);
    GETPROCADDR(GetDeviceQueue);
    GETPROCADDR(QueueSubmit);
    GETPROCADDR(QueueSubmit2);

    // if (!strcmp(pName, "vkBindBufferMemory2") ||
    //     !strcmp(pName, "vkBindBufferMemory2KHR")) {
    //     return (PFN_vkVoidFunction)&DxvkMaliCompatLayer_BindBufferMemory2;
    // }

    {
        scoped_lock l(global_lock);
        struct device *dev = get_device(device);
        if (!dev)
            return NULL;

        return dev->table.GetDeviceProcAddr(device, pName);
    }
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL
DxvkMaliCompatLayer_GetInstanceProcAddr(VkInstance instance,
                                        const char *pName) {
    if (!strcmp(pName, "vkGetPhysicalDeviceProperties2") ||
        !strcmp(pName, "vkGetPhysicalDeviceProperties2KHR")) {
        return (
            PFN_vkVoidFunction)&DxvkMaliCompatLayer_GetPhysicalDeviceProperties2;
    }
    if (!strcmp(pName, "vkGetPhysicalDeviceFeatures2") ||
        !strcmp(pName, "vkGetPhysicalDeviceFeatures2KHR")) {
        return (
            PFN_vkVoidFunction)&DxvkMaliCompatLayer_GetPhysicalDeviceFeatures2;
    }
    GETPROCADDR(CreateInstance);
    GETPROCADDR(EnumeratePhysicalDevices)
    GETPROCADDR(GetPhysicalDeviceFeatures);
    GETPROCADDR(EnumerateDeviceExtensionProperties);
    GETPROCADDR(DestroyInstance);
    GETPROCADDR(CreateDevice);

    {
        scoped_lock l(global_lock);
        VkLayerInstanceDispatchTable table = instanceDispatch[GetKey(instance)];
        return table.GetInstanceProcAddr(instance, pName);
    }
}
