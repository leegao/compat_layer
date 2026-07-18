#include "layer.hpp"

#include "buffer.hpp"
#include "image.hpp"
#include "logger.hpp"
#include "null_descriptors.hpp"
#include "sparse_binding.hpp"
#include "spoof_profile.hpp"
#include "vk_func.hpp"
#include "vulkan/vk_layer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unistd.h>
#include <unordered_set>
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
std::unordered_map<void *, std::unordered_set<std::string>> deviceExtensionsMap;

std::mutex global_lock;

void *GetInstanceKey(VkPhysicalDevice physicalDevice) {
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
    table.GetPhysicalDeviceSparseImageFormatProperties2 =
        (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2)gip(
            *pInstance, "vkGetPhysicalDeviceSparseImageFormatProperties2");
    table.GetPhysicalDeviceQueueFamilyProperties2 =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties2)gip(
            *pInstance, "vkGetPhysicalDeviceQueueFamilyProperties2");

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
        VkPhysicalDevice pd = pPhysicalDevices[index];
        VkPhysicalDeviceFeatures features{};
        instanceDispatch[GetKey(instance)].GetPhysicalDeviceFeatures(pd,
                                                                     &features);

        VkPhysicalDeviceDriverProperties driverProperties{};
        VkPhysicalDeviceProperties2 props2{};
        driverProperties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &driverProperties;
        instanceDispatch[GetKey(instance)].GetPhysicalDeviceProperties2(
            pd, &props2);

        featuresMap[GetKey(pd)] = features;
        propertiesMap[GetKey(pd)] = props2;
        driverPropertiesMap[GetKey(pd)] = driverProperties;

        uint32_t extCount = 0;
        instanceDispatch[GetKey(instance)].EnumerateDeviceExtensionProperties(
            pd, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> extProps(extCount);
        instanceDispatch[GetKey(instance)].EnumerateDeviceExtensionProperties(
            pd, nullptr, &extCount, extProps.data());

        auto &extSet = deviceExtensionsMap[GetKey(pd)];
        extSet.clear();
        for (const auto &ext : extProps) {
            extSet.insert(ext.extensionName);
        }
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_GetPhysicalDeviceFeatures(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures *pFeatures) {
    scoped_lock l(global_lock);

    instanceDispatch[GetInstanceKey(physicalDevice)].GetPhysicalDeviceFeatures(
        physicalDevice, pFeatures);

    spoof_core_features(pFeatures);
}

template <typename DstType, typename SrcType>
void CopyDescriptorIndexingFields(DstType *dst, const SrcType *src) {
    dst->shaderUniformTexelBufferArrayDynamicIndexing =
        src->shaderUniformTexelBufferArrayDynamicIndexing;
    dst->shaderStorageTexelBufferArrayDynamicIndexing =
        src->shaderStorageTexelBufferArrayDynamicIndexing;
    dst->shaderSampledImageArrayNonUniformIndexing =
        src->shaderSampledImageArrayNonUniformIndexing;
    dst->shaderStorageImageArrayNonUniformIndexing =
        src->shaderStorageImageArrayNonUniformIndexing;
    dst->shaderStorageBufferArrayNonUniformIndexing =
        src->shaderStorageBufferArrayNonUniformIndexing;
    dst->shaderUniformTexelBufferArrayNonUniformIndexing =
        src->shaderUniformTexelBufferArrayNonUniformIndexing;
    dst->shaderStorageTexelBufferArrayNonUniformIndexing =
        src->shaderStorageTexelBufferArrayNonUniformIndexing;
    dst->descriptorBindingSampledImageUpdateAfterBind =
        src->descriptorBindingSampledImageUpdateAfterBind;
    dst->descriptorBindingStorageImageUpdateAfterBind =
        src->descriptorBindingStorageImageUpdateAfterBind;
    dst->descriptorBindingStorageBufferUpdateAfterBind =
        src->descriptorBindingStorageBufferUpdateAfterBind;
    dst->descriptorBindingUniformTexelBufferUpdateAfterBind =
        src->descriptorBindingUniformTexelBufferUpdateAfterBind;
    dst->descriptorBindingStorageTexelBufferUpdateAfterBind =
        src->descriptorBindingStorageTexelBufferUpdateAfterBind;
    dst->descriptorBindingUpdateUnusedWhilePending =
        src->descriptorBindingUpdateUnusedWhilePending;
    dst->descriptorBindingPartiallyBound = src->descriptorBindingPartiallyBound;
    dst->descriptorBindingVariableDescriptorCount =
        src->descriptorBindingVariableDescriptorCount;
    dst->runtimeDescriptorArray = src->runtimeDescriptorArray;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_GetPhysicalDeviceFeatures2(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures) {
    scoped_lock l(global_lock);

    auto instanceKey = GetInstanceKey(physicalDevice);
    instanceDispatch[instanceKey].GetPhysicalDeviceFeatures2(physicalDevice,
                                                             pFeatures);

    // Alias VK_EXT_descriptor_indexing to VK_1_2 features
    VkPhysicalDeviceProperties2 physProps =
        propertiesMap[GetKey(physicalDevice)];
    if (physProps.properties.apiVersion < VK_API_VERSION_1_2) {
        const auto &supportedExtensions =
            deviceExtensionsMap[GetKey(physicalDevice)];
        bool driverSupportsIndexing =
            supportedExtensions.find(
                VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) !=
            supportedExtensions.end();

        VkBaseOutStructure *ext =
            reinterpret_cast<VkBaseOutStructure *>(pFeatures->pNext);
        while (ext) {
            if (ext->sType ==
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
                auto *vk12Features =
                    reinterpret_cast<VkPhysicalDeviceVulkan12Features *>(ext);

                if (driverSupportsIndexing) {
                    VkPhysicalDeviceDescriptorIndexingFeaturesEXT extIndexing{
                        .sType =
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
                    VkPhysicalDeviceFeatures2 queryFeatures{
                        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                        .pNext = &extIndexing};
                    instanceDispatch[instanceKey].GetPhysicalDeviceFeatures2(
                        physicalDevice, &queryFeatures);
                    CopyDescriptorIndexingFields(vk12Features, &extIndexing);
                    vk12Features->descriptorIndexing = VK_TRUE;
                } else {
                    vk12Features->descriptorIndexing = VK_FALSE;
                }
            }
            ext = ext->pNext;
        }
    }

    spoof_core_features(&pFeatures->features);
    spoof_next_features(reinterpret_cast<VkBaseOutStructure *>(pFeatures));
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_GetPhysicalDeviceProperties(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties *pProperties) {
    scoped_lock l(global_lock);

    instanceDispatch[GetInstanceKey(physicalDevice)]
        .GetPhysicalDeviceProperties(physicalDevice, pProperties);
    spoof_core_properties(pProperties);
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_GetPhysicalDeviceProperties2(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2 *pProperties) {
    scoped_lock l(global_lock);

    instanceDispatch[GetInstanceKey(physicalDevice)]
        .GetPhysicalDeviceProperties2(physicalDevice, pProperties);
    spoof_core_properties(&pProperties->properties);
    spoof_properties(reinterpret_cast<VkBaseOutStructure *>(pProperties));
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DxvkMaliCompatLayer_EnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice, const char *pLayerName,
    uint32_t *pPropertyCount, VkExtensionProperties *pProperties) {
    scoped_lock l(global_lock);

    auto instanceKey = GetInstanceKey(physicalDevice);
    if (!instanceKey)
        return VK_ERROR_INITIALIZATION_FAILED;
    auto &table = instanceDispatch[instanceKey];

    if (pLayerName != nullptr) {
        return table.EnumerateDeviceExtensionProperties(
            physicalDevice, pLayerName, pPropertyCount, pProperties);
    }

    auto spoofed = get_unsupported_spoofed_extensions(physicalDevice);
    if (pProperties == nullptr) {
        uint32_t count = 0;
        VkResult result = table.EnumerateDeviceExtensionProperties(
            physicalDevice, nullptr, &count, nullptr);
        if (result == VK_SUCCESS) {
            *pPropertyCount = count + static_cast<uint32_t>(spoofed.size());
        }
        return result;
    }

    uint32_t capacity = *pPropertyCount;
    if (capacity == 0) {
        return VK_SUCCESS;
    }

    uint32_t actualCount = 0;
    VkResult result = table.EnumerateDeviceExtensionProperties(
        physicalDevice, nullptr, &actualCount, nullptr);
    if (result != VK_SUCCESS) {
        return result;
    }

    std::vector<VkExtensionProperties> actualProps(actualCount);
    result = table.EnumerateDeviceExtensionProperties(
        physicalDevice, nullptr, &actualCount, actualProps.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        return result;
    }

    std::vector<VkExtensionProperties> combined = actualProps;
    for (const auto &ext : spoofed) {
        combined.push_back(ext);
    }

    uint32_t copyCount =
        std::min(capacity, static_cast<uint32_t>(combined.size()));
    std::memcpy(pProperties, combined.data(),
                copyCount * sizeof(VkExtensionProperties));

    *pPropertyCount = copyCount;

    if (copyCount < combined.size()) {
        return VK_INCOMPLETE;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_GetBufferMemoryRequirements(
    VkDevice device, VkBuffer buffer,
    VkMemoryRequirements *pMemoryRequirements) {
    struct device *dev = get_device(device);
    if (dev && dev->emulate_sparse_binding) {
        if (auto *res = find_sparse_buffer(buffer)) {
            GetSparseBufferMemoryRequirements(res, pMemoryRequirements);
            return;
        }
    }
    dev->table.GetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_GetBufferMemoryRequirements2(
    VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo,
    VkMemoryRequirements2 *pMemoryRequirements) {
    struct device *dev = get_device(device);
    if (dev && dev->emulate_sparse_binding) {
        if (auto *res = find_sparse_buffer(pInfo->buffer)) {
            GetSparseBufferMemoryRequirements(
                res, &pMemoryRequirements->memoryRequirements);
            return;
        }
    }
    dev->table.GetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_GetImageSparseMemoryRequirements(
    VkDevice device, VkImage image, uint32_t *pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements *pSparseMemoryRequirements) {
    struct device *dev = get_device(device);
    if (dev && dev->emulate_sparse_binding) {
        if (auto *res = find_sparse_image(image)) {
            GetSparseImageMemoryRequirements(res, pSparseMemoryRequirementCount,
                                             pSparseMemoryRequirements);
            return;
        }
    }
    dev->table.GetImageSparseMemoryRequirements(device, image,
                                                pSparseMemoryRequirementCount,
                                                pSparseMemoryRequirements);
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_GetPhysicalDeviceSparseImageFormatProperties2(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo,
    uint32_t *pPropertyCount, VkSparseImageFormatProperties2 *pProperties) {
    if (pFormatInfo->type == VK_IMAGE_TYPE_2D &&
        pFormatInfo->samples == VK_SAMPLE_COUNT_1_BIT) {
        if (!pProperties) {
            *pPropertyCount = 1;
            return;
        }

        if (*pPropertyCount == 0) {
            return;
        }

        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        if (pFormatInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        *pPropertyCount = 1;
        pProperties[0].sType =
            VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2;
        pProperties[0].properties.aspectMask = aspectMask;
        pProperties[0].properties.imageGranularity =
            GetBlockShape(pFormatInfo->format);
        pProperties[0].properties.flags =
            VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;
        return;
    }

    instanceDispatch[GetInstanceKey(physicalDevice)]
        .GetPhysicalDeviceSparseImageFormatProperties2(
            physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_GetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount,
    VkQueueFamilyProperties *pQueueFamilyProperties) {
    instanceDispatch[GetInstanceKey(physicalDevice)]
        .GetPhysicalDeviceQueueFamilyProperties(
            physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);

    if (!pQueueFamilyProperties)
        return;

    for (int i = 0; i < *pQueueFamilyPropertyCount; i++) {
        auto &flags = pQueueFamilyProperties[i].queueFlags;
        if (flags & VK_QUEUE_COMPUTE_BIT) {
            flags |= VK_QUEUE_SPARSE_BINDING_BIT;
            break;
        }
    }
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_GetPhysicalDeviceQueueFamilyProperties2(
    VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount,
    VkQueueFamilyProperties2 *pQueueFamilyProperties) {
    instanceDispatch[GetInstanceKey(physicalDevice)]
        .GetPhysicalDeviceQueueFamilyProperties2(
            physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);

    if (!pQueueFamilyProperties)
        return;

    for (int i = 0; i < *pQueueFamilyPropertyCount; i++) {
        auto &flags =
            pQueueFamilyProperties[i].queueFamilyProperties.queueFlags;
        if (flags & VK_QUEUE_COMPUTE_BIT) {
            flags |= VK_QUEUE_SPARSE_BINDING_BIT;
            break;
        }
    }
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

bool CheckForPushDescriptorSupport(VkInstance instance,
                                   VkPhysicalDevice physicalDevice) {
    VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProps = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR,
    };
    VkPhysicalDeviceProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &pushDescriptorProps,
    };

    instanceDispatch[GetKey(instance)].GetPhysicalDeviceProperties2(
        physicalDevice, &properties);

    return pushDescriptorProps.maxPushDescriptors > 0;
}

VkPhysicalDeviceRobustness2FeaturesEXT
CheckForRobustness2Support(VkInstance instance,
                           VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceRobustness2FeaturesEXT extFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
    };
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &extFeatures,
    };

    instanceDispatch[GetKey(instance)].GetPhysicalDeviceFeatures2(
        physicalDevice, &features);

    return extFeatures;
}

bool CheckForImageRobustnessSupport(VkInstance instance,
                                    VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceImageRobustnessFeaturesEXT extFeatures = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT,
    };
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &extFeatures,
    };

    instanceDispatch[GetKey(instance)].GetPhysicalDeviceFeatures2(
        physicalDevice, &features);

    return extFeatures.robustImageAccess;
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
    bool hasPushDescriptorSupport =
        CheckForPushDescriptorSupport(instance, physicalDevice);
    auto queriedRobustness2Features =
        CheckForRobustness2Support(instance, physicalDevice);
    bool hasNullDescriptorSupport = queriedRobustness2Features.nullDescriptor;
    bool hasImageRobustnessSupport =
        CheckForImageRobustnessSupport(instance, physicalDevice);

    auto emulate_push_descriptors =
        getenv("COMPAT_EMULATE_PUSH_DESCRIPTORS")
            ? atoi(getenv("COMPAT_EMULATE_PUSH_DESCRIPTORS"))
            : !hasPushDescriptorSupport;

    if (emulate_push_descriptors) {
        Logger::log("info", "Emulating VK_KHR_push_descriptor");
    }

    auto emulate_precise_null_descriptor =
        getenv("COMPAT_EMULATE_PRECISE_NULL_DESCRIPTORS")
            ? atoi(getenv("COMPAT_EMULATE_PRECISE_NULL_DESCRIPTORS"))
            : 0;

    auto emulate_null_descriptor =
        getenv("COMPAT_EMULATE_NULL_DESCRIPTORS")
            ? atoi(getenv("COMPAT_EMULATE_NULL_DESCRIPTORS"))
            : !hasNullDescriptorSupport || emulate_precise_null_descriptor;

    if (emulate_null_descriptor) {
        Logger::log(
            "info",
            "Emulating VK_EXT_robustness2::nullDescriptor, precision = %d",
            emulate_precise_null_descriptor);
    }

    // Make sparse binding a forced toggle instead of inferred
    auto emulate_sparse_binding =
        getenv("COMPAT_EMULATE_SPARSE_BINDING")
            ? atoi(getenv("COMPAT_EMULATE_SPARSE_BINDING"))
            : 0; // featuresMap[GetKey(physicalDevice)].sparseBinding

    VkBaseOutStructure *ext = (VkBaseOutStructure *)createInfo.pNext;
    VkPhysicalDeviceFeatures2 *appFeatures2 = nullptr;
    VkPhysicalDeviceFaultFeaturesEXT *appFaultFeatures = nullptr;
    VkPhysicalDeviceImageRobustnessFeaturesEXT *appRobustnessFeatures = nullptr;
    VkPhysicalDeviceVulkan13Features *vulkan13Features = nullptr;
    VkPhysicalDeviceVulkan12Features *vulkan12Features = nullptr;

    while (ext) {
        if (ext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
            appFeatures2 = (VkPhysicalDeviceFeatures2 *)ext;
        } else if (ext->sType ==
                   VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT) {
            appFaultFeatures = (VkPhysicalDeviceFaultFeaturesEXT *)ext;
        } else if (
            ext->sType ==
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT) {
            appRobustnessFeatures =
                (VkPhysicalDeviceImageRobustnessFeaturesEXT *)ext;
        } else if (ext->sType ==
                   VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
            vulkan13Features = (VkPhysicalDeviceVulkan13Features *)ext;
        } else if (ext->sType ==
                   VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
            vulkan12Features = (VkPhysicalDeviceVulkan12Features *)ext;
        }
        ext = ext->pNext;
    }

    VkPhysicalDeviceProperties2 physProps =
        propertiesMap[GetKey(physicalDevice)];
    uint32_t apiVersion = physProps.properties.apiVersion;

    auto actualExtensions = get_actual_device_extensions(physicalDevice);
    std::vector<const char *> enabledExtensions;
    if (createInfo.ppEnabledExtensionNames &&
        createInfo.enabledExtensionCount > 0) {
        for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i) {
            const char *extName = createInfo.ppEnabledExtensionNames[i];
            bool is_spoofed = false;
            for (size_t k = 0; k < SPOOFED_EXTENSIONS_COUNT; ++k) {
                if (SPOOFED_EXTENSIONS[k].name == extName) {
                    is_spoofed = true;
                    break;
                }
            }

            if (is_spoofed) {
                if (actualExtensions.find(extName) != actualExtensions.end()) {
                    enabledExtensions.push_back(extName);
                } else {
                    Logger::log("info", "Masking extension: %s", extName);
                }
            } else {
                enabledExtensions.push_back(extName);
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

    if (emulate_null_descriptor && hasImageRobustnessSupport &&
        !hasExtension(VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME)) {
        Logger::log("info",
                    "Adding extension " VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME
                    " for VK_EXT_null_descriptor emulation");
        enabledExtensions.push_back(VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME);
    }

    createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(enabledExtensions.size());

    VkPhysicalDeviceFaultFeaturesEXT layerFaultFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT,
        .deviceFault = VK_TRUE,
        .deviceFaultVendorBinary = queriedFaultFeatures.deviceFaultVendorBinary,
    };

    VkPhysicalDeviceImageRobustnessFeaturesEXT layerRobustnessFeatures = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT,
        .robustImageAccess = VK_TRUE,
    };

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT layerIndexingFeatures{
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};

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

    if (emulate_null_descriptor && hasImageRobustnessSupport) {
        if (vulkan13Features) {
            vulkan13Features->robustImageAccess = VK_TRUE;
        } else if (appRobustnessFeatures) {
            appRobustnessFeatures->robustImageAccess = VK_TRUE;
        } else {
            Logger::log("info", "Enabling VK_EXT_image_robustness features for "
                                "VK_EXT_robustness2 emulation");
            layerRobustnessFeatures.pNext = (void *)createInfo.pNext;
            createInfo.pNext = &layerRobustnessFeatures;
        }
    } else if (emulate_null_descriptor) {
        Logger::log("error", "VK_EXT_image_robustness not supported, "
                             "VK_EXT_robustness2 emulation may be broken");
    }

    bool aliasDescriptorIndexing = false;
    if (apiVersion < VK_API_VERSION_1_2 && vulkan12Features != nullptr) {
        aliasDescriptorIndexing = vulkan12Features->descriptorIndexing;
        VkBaseOutStructure *curr =
            reinterpret_cast<VkBaseOutStructure *>(&createInfo);
        while (curr->pNext) {
            VkBaseOutStructure *next =
                reinterpret_cast<VkBaseOutStructure *>(curr->pNext);
            if (next->sType ==
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
                curr->pNext = next->pNext; // detach vulkan12Features
                break;
            }
            curr = next;
        }
    }

    if (apiVersion < VK_API_VERSION_1_3 && vulkan13Features != nullptr) {
        VkBaseOutStructure *curr =
            reinterpret_cast<VkBaseOutStructure *>(&createInfo);
        while (curr->pNext) {
            VkBaseOutStructure *next =
                reinterpret_cast<VkBaseOutStructure *>(curr->pNext);
            if (next->sType ==
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
                curr->pNext = next->pNext; // detach vulkan13Features
                break;
            }
            curr = next;
        }
    }

    if (aliasDescriptorIndexing &&
        !hasExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
        Logger::log(
            "info",
            "Adding extension " VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        enabledExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

        if (aliasDescriptorIndexing) {
            Logger::log("info",
                        "Enabling " VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
                        " features");
            CopyDescriptorIndexingFields(&layerIndexingFeatures,
                                         vulkan12Features);
            layerIndexingFeatures.pNext = const_cast<void *>(createInfo.pNext);
            createInfo.pNext = &layerIndexingFeatures;
        }
    }

    VkPhysicalDeviceFeatures &actualCoreFeatures =
        featuresMap[GetKey(physicalDevice)];

    VkPhysicalDeviceFeatures mutableCoreFeatures{};
    if (createInfo.pEnabledFeatures) {
        mutableCoreFeatures = *createInfo.pEnabledFeatures;
        mask_core_features(&mutableCoreFeatures, &actualCoreFeatures);
        createInfo.pEnabledFeatures = &mutableCoreFeatures;
    }

    if (emulate_null_descriptor && actualCoreFeatures.robustBufferAccess) {
        Logger::log("info", "Enabling robustBufferAccess feature for "
                            "VK_EXT_robustness2 emulation");
        mutableCoreFeatures.robustBufferAccess = VK_TRUE;
        if (appFeatures2)
            appFeatures2->features.robustBufferAccess = VK_TRUE;
    } else if (emulate_null_descriptor) {
        Logger::log("error", "robustBufferAccess not supported, "
                             "VK_EXT_robustness2 emulation may be broken");
    }

    if (emulate_sparse_binding) {
        Logger::log("info", "Emulating (dense) sparse binding");
        mutableCoreFeatures.sparseBinding = VK_TRUE;
        if (appFeatures2)
            appFeatures2->features.sparseBinding = VK_TRUE;
    }

    mask_next_features(
        physicalDevice,
        const_cast<VkBaseOutStructure *>(
            reinterpret_cast<const VkBaseOutStructure *>(createInfo.pNext)));

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
    device->memoryProps = memoryProps;
    device->table = table;
    device->memoryIndex = memoryIndex;
    device->queue = queue;
    device->queueFamilyIndex = i;
    device->alloc = pAllocator;
    device->profile_transfers = profile_transfers;
    device->sample_gpu_counters = sample_gpu_counters;
    device->has_more_layers = has_more_layers;
    device->emulate_push_descriptors = emulate_push_descriptors;
    device->emulate_null_descriptor = emulate_null_descriptor;
    device->emulate_precise_null_descriptor = emulate_precise_null_descriptor;
    device->emulate_sparse_binding = emulate_sparse_binding;

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

    create_null_resources(device.get());
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

    {
        scoped_lock l(global_lock);
        for (auto &stagingResources : dev->stagingResourcesQueue) {
            stagingResources->Cleanup();
        }
        dev->stagingResourcesQueue.clear();
        dev->syncPool.reset();
        dev->descriptorSetAllocator->cleanup();
        dev->descriptorSetAllocator.reset();
    }
    destroy_null_resources(dev);

    if (device != VK_NULL_HANDLE)
        dev->table.DestroyDevice(device, pAllocator);

    deviceMap.erase(GetKey(device));
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL
DxvkMaliCompatLayer_GetDeviceProcAddr(VkDevice device, const char *pName) {
    GETPROCADDR(DestroyDevice);

#ifdef ENABLE_BUFFER_TRACKING
    GETPROCADDR(CreateBuffer);
    GETPROCADDR(BindBufferMemory);
    GETPROCADDR(BindBufferMemory2);
    GETPROCADDR(DestroyBuffer);
#endif
    GETPROCADDR(CreateImage);
    GETPROCADDR(CreateImageView);
    GETPROCADDR(DestroyImage);

    GETPROCADDR(AllocateCommandBuffers);
    GETPROCADDR(FreeCommandBuffers);
    GETPROCADDR(BeginCommandBuffer);
    GETPROCADDR(ResetCommandBuffer);
    GETPROCADDR(ResetCommandPool);
    GETPROCADDR(DestroyCommandPool);
    // GETPROCADDR(CmdBindPipeline);
    // GETPROCADDR(CmdBindDescriptorSets);
    // GETPROCADDR(CmdBindDescriptorSets2);
    // GETPROCADDR(CmdPushConstants);
    // GETPROCADDR(CmdPushConstants2);
    GETPROCADDR(CreateDescriptorSetLayout);
    GETPROCADDR(DestroyDescriptorSetLayout);
    GETPROCADDR(CreatePipelineLayout);
    GETPROCADDR(DestroyPipelineLayout);
    // GETPROCADDR(CreateGraphicsPipelines);
    // GETPROCADDR(CreateComputePipelines);
    // GETPROCADDR(DestroyPipeline);
    GETPROCADDR(GetDeviceQueue);
    GETPROCADDR(QueueSubmit);
    GETPROCADDR(QueueSubmit2);
    GETPROCADDR(CreateShaderModule);
    GETPROCADDR(DestroyShaderModule);

    GETPROCADDR(CmdPushDescriptorSetKHR);
    GETPROCADDR(CmdPushDescriptorSetWithTemplateKHR);
    if (!strcmp(pName, "vkCreateDescriptorUpdateTemplate") ||
        !strcmp(pName, "vkCreateDescriptorUpdateTemplateKHR")) {
        return (
            PFN_vkVoidFunction)&DxvkMaliCompatLayer_CreateDescriptorUpdateTemplate;
    }
    if (!strcmp(pName, "vkDestroyDescriptorUpdateTemplate") ||
        !strcmp(pName, "vkDestroyDescriptorUpdateTemplateKHR")) {
        return (
            PFN_vkVoidFunction)&DxvkMaliCompatLayer_DestroyDescriptorUpdateTemplate;
    }
    GETPROCADDR(UpdateDescriptorSets);
    if (!strcmp(pName, "vkUpdateDescriptorSetWithTemplate") ||
        !strcmp(pName, "vkUpdateDescriptorSetWithTemplateKHR")) {
        return (
            PFN_vkVoidFunction)&DxvkMaliCompatLayer_UpdateDescriptorSetWithTemplate;
    }

    if (!strcmp(pName, "vkBindBufferMemory2") ||
        !strcmp(pName, "vkBindBufferMemory2KHR")) {
        return (PFN_vkVoidFunction)&DxvkMaliCompatLayer_BindBufferMemory2;
    }

    GETPROCADDR(GetBufferMemoryRequirements);
    GETPROCADDR(GetBufferMemoryRequirements2);
    GETPROCADDR(GetImageSparseMemoryRequirements);
    GETPROCADDR(QueueBindSparse);

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
    GETPROCADDR(GetInstanceProcAddr); // Layers have to also hook their own
                                      // GetInstanceProcAddr
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
    GETPROCADDR(GetPhysicalDeviceProperties);
    GETPROCADDR(GetPhysicalDeviceFeatures);
    GETPROCADDR(EnumerateDeviceExtensionProperties);
    GETPROCADDR(DestroyInstance);
    GETPROCADDR(CreateDevice);
    GETPROCADDR(GetPhysicalDeviceQueueFamilyProperties);
    GETPROCADDR(GetPhysicalDeviceQueueFamilyProperties2);
    GETPROCADDR(GetPhysicalDeviceSparseImageFormatProperties2);

    {
        scoped_lock l(global_lock);
        VkLayerInstanceDispatchTable table = instanceDispatch[GetKey(instance)];
        return table.GetInstanceProcAddr(instance, pName);
    }
}
