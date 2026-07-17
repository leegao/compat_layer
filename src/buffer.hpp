#pragma once

#include <vulkan/vulkan.h>

#include <string_view>

struct device;

struct buffer {
    VkBuffer handle;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize offset;
    VkDeviceAddress deviceAddress = 0;

    struct device *device;
    const VkAllocationCallbacks *alloc;
    std::string_view label;
    int id;
    bool owns_memory = false;
};

struct buffer *find_buffer(VkBuffer);

struct buffer *CreateStagingBuffer(
    device *dev, VkDeviceSize size, std::string_view label,
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VkDeviceMemory memory = VK_NULL_HANDLE);

uint32_t FindMemoryType(struct device *dev, uint32_t typeBits);
