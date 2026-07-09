#include "buffer.hpp"
#include "layer.hpp"
#include "logger.hpp"

#include <atomic>
#include <unordered_map>

std::unordered_map<VkBuffer, std::unique_ptr<struct buffer>> buffersMap;

std::atomic<int> bufferIdCounter;

struct buffer *find_buffer(VkBuffer buffer) {
    auto it = buffersMap.find(buffer);

    if (it == buffersMap.end())
        return nullptr;

    return it->second.get();
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateBuffer(
    VkDevice device, const VkBufferCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer) {
    VkResult result;
    VkLayerDispatchTable table;
    VkBufferCreateInfo create_info = *pCreateInfo;

    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_INITIALIZATION_FAILED;

    table = dev->table;

    result = table.CreateBuffer(device, &create_info, pAllocator, pBuffer);
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
    buffersMap.erase(buffer);
}
