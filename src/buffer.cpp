#include "buffer.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "sparse_binding.hpp"
#include "vk_func.hpp"
#include <atomic>
#include <unordered_map>
#include <vulkan/vulkan.h>

std::unordered_map<VkBuffer, std::unique_ptr<struct buffer>> buffersMap;

std::atomic<int> bufferIdCounter;

#ifdef ENABLE_BUFFER_TRACKING

uint32_t FindMemoryType(struct device *dev, uint32_t typeBits,
                        VkMemoryPropertyFlags required) {
    if (typeBits & (1u << dev->memoryIndex)) {
        return dev->memoryIndex;
    }
    for (uint32_t i = 0; i < dev->memoryProps.memoryTypeCount; ++i) {
        if (typeBits & (1u << i) &&
            (dev->memoryProps.memoryTypes[i].propertyFlags & required)) {
            return i;
        }
    }
    return dev->memoryIndex;
}

struct buffer *find_buffer(VkBuffer buffer) {
    auto it = buffersMap.find(buffer);
    if (it == buffersMap.end())
        return nullptr;
    return it->second.get();
}

struct dense_sparse_resource *find_sparse_buffer(VkBuffer buffer) {
    auto buf = find_buffer(buffer);
    return buf && buf->emulate_sparse_binding ? buf->sparse_resource.get()
                                              : nullptr;
}

struct buffer *CreateStagingBuffer(device *dev, VkDeviceSize size,
                                   std::string_view label,
                                   VkBufferUsageFlags usage,
                                   VkDeviceMemory memory) {
    size = (size + 15) & ~15;
    VkBufferCreateInfo bufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult result = DxvkMaliCompatLayer_CreateBuffer(
        dev->handle, &bufferCreateInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        Logger::log(
            "error",
            "create_staging_buffer: failed to create staging buffer: %d",
            result);
        return nullptr;
    }

    auto stagingBuf = find_buffer(buffer);
    if (!stagingBuf) {
        Logger::log(
            "error",
            "create_staging_buffer: failed to create staging buffer: %d",
            result);
        return nullptr;
    }

    bool hasMemory = memory != VK_NULL_HANDLE;
    if (!hasMemory) {
        VkMemoryRequirements reqs;
        dev->table.GetBufferMemoryRequirements(dev->handle, buffer, &reqs);
        VkMemoryAllocateInfo memoryAllocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = reqs.size,
            .memoryTypeIndex = FindMemoryType(dev, reqs.memoryTypeBits),
        };

        result = dev->table.AllocateMemory(dev->handle, &memoryAllocInfo,
                                           nullptr, &memory);
        if (result != VK_SUCCESS) {
            Logger::log(
                "error",
                "create_staging_buffer: failed to allocate staging memory: %d",
                result);
            DxvkMaliCompatLayer_DestroyBuffer(dev->handle, buffer, nullptr);
            return nullptr;
        }
        stagingBuf->owns_memory = true;
    }

    result =
        DxvkMaliCompatLayer_BindBufferMemory(dev->handle, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        Logger::log("error", "create_staging_buffer: failed to bind memory: %d",
                    result);
        if (!hasMemory)
            dev->table.FreeMemory(dev->handle, memory, nullptr);
        DxvkMaliCompatLayer_DestroyBuffer(dev->handle, buffer, nullptr);
        return nullptr;
    }

    stagingBuf->label = label;
    stagingBuf->id = bufferIdCounter.fetch_add(1);
    return stagingBuf;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateBuffer(
    VkDevice device, const VkBufferCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer) {
    VkResult result;
    VkBufferCreateInfo create_info = *pCreateInfo;

    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_UNKNOWN;

    static constexpr VkBufferCreateFlags kSparseFlags =
        VK_BUFFER_CREATE_SPARSE_BINDING_BIT |
        VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT |
        VK_BUFFER_CREATE_SPARSE_ALIASED_BIT;

    bool emulate_sparse_binding =
        dev->emulate_sparse_binding && (pCreateInfo->flags & kSparseFlags);
    if (emulate_sparse_binding) {
        create_info.flags &= ~kSparseFlags;
    }

    result = dev->table.CreateBuffer(device, &create_info, pAllocator, pBuffer);
    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to create buffer, res %d", result);
        return result;
    }

    auto buf = std::make_unique<struct buffer>();
    buf->handle = *pBuffer;
    buf->size = pCreateInfo->size;
    buf->device = dev;
    buf->alloc = pAllocator;
    buf->id = 0;

    if (emulate_sparse_binding) {
        result = BindSparseBuffer(dev, pCreateInfo, pAllocator, buf.get());
        if (result != VK_SUCCESS) {
            Logger::log("error", "Failed to bind sparse buffer, res %d",
                        result);
            return result;
        }
    }

    {
        scoped_lock l(global_lock);
        buffersMap[*pBuffer] = std::move(buf);
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_BindBufferMemory(
    VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
    VkDeviceSize memoryOffset) {
    VkResult result;
    VkLayerDispatchTable table;

    scoped_lock l(global_lock);

    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_INITIALIZATION_FAILED;

    table = dev->table;

    result = table.BindBufferMemory(device, buffer, memory, memoryOffset);
    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to bind buffer memory, res %d", result);
        return result;
    }

    struct buffer *buf = find_buffer(buffer);
    if (!buf) {
        Logger::log("error", "BindBufferMemory: Buffer %p not found", buffer);
        return VK_SUCCESS;
    }

    buf->memory = memory;
    buf->offset = memoryOffset;

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_BindBufferMemory2(
    VkDevice device, uint32_t bindInfoCount,
    const VkBindBufferMemoryInfo *pBindInfos) {
    VkResult result;
    VkLayerDispatchTable table;

    scoped_lock l(global_lock);

    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_INITIALIZATION_FAILED;

    table = dev->table;

    // Don't emulate with BindBufferMemory in case pBindInfos has a pNext
    result = table.BindBufferMemory2(device, bindInfoCount, pBindInfos);
    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to bind buffer memory, res %d", result);
        return result;
    }

    for (uint32_t i = 0; i < bindInfoCount; i++) {
        struct buffer *buf = find_buffer(pBindInfos[i].buffer);

        if (!buf) {
            Logger::log("error", "BindBufferMemory2: Buffer %p not found",
                        pBindInfos[i].buffer);
            continue;
        }
        buf->memory = pBindInfos[i].memory;
        buf->offset = pBindInfos[i].memoryOffset;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyBuffer(
    VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *pAllocator) {
    scoped_lock l(global_lock);

    struct device *dev = get_device(device);
    struct buffer *buf = find_buffer(buffer);
    if (!dev || !buf)
        return;

    dev->table.DestroyBuffer(device, buffer, pAllocator);
    if (buf->emulate_sparse_binding) {
        DestroySparseBuffer(dev, buffer, pAllocator);
    }
    buffersMap.erase(buffer);
}

#endif
