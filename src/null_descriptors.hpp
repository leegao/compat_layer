#pragma once

#include <vulkan/vulkan.h>

struct device;

struct null_descriptor_emulation {
    VkBuffer null_buffer = VK_NULL_HANDLE;
    VkDeviceMemory null_buffer_memory = VK_NULL_HANDLE;
    VkBufferView null_buffer_view = VK_NULL_HANDLE;
    VkImage null_image = VK_NULL_HANDLE;
    VkDeviceMemory null_image_memory = VK_NULL_HANDLE;
    VkImageView null_image_view = VK_NULL_HANDLE;
    VkSampler null_sampler = VK_NULL_HANDLE;
};

void create_null_resources(struct device *dev);

void destroy_null_resources(struct device *dev);
