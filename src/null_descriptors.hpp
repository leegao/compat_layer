#pragma once

#include "descriptors.hpp"
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <vulkan/vulkan.h>

struct device;

struct null_image_resource {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

using ImageShapeKey = uint64_t;
inline ImageShapeKey GetImageShapeKey(VkImageViewType viewType, VkFormat format,
                                      VkSampleCountFlagBits samples) {
    return (static_cast<ImageShapeKey>(viewType) << 44) |
           (static_cast<ImageShapeKey>(format) << 8) |
           static_cast<ImageShapeKey>(samples);
}

struct null_descriptor_emulation {
    VkBuffer null_buffer = VK_NULL_HANDLE;
    VkDeviceMemory null_buffer_memory = VK_NULL_HANDLE;
    VkBufferView null_buffer_view = VK_NULL_HANDLE; // R32_UINT
    null_image_resource null_image_rgba8_2d;        // 2D 1x1
    VkSampler null_sampler = VK_NULL_HANDLE;

    std::shared_mutex cacheLock;
    std::unordered_map<VkFormat, VkBufferView> bufferViewsByFormat;
    std::unordered_map<ImageShapeKey, null_image_resource> imagesByShape;
};

void create_null_resources(struct device *dev);

void destroy_null_resources(struct device *dev);

VkBufferView get_null_buffer_view(struct device *dev, VkFormat format);

VkImageView get_null_image_view(struct device *dev, VkImageViewType viewType,
                                VkFormat format, VkSampleCountFlagBits samples);

bool fix_null_descriptors(struct device *dev, uint32_t updatesCount,
                          const VkWriteDescriptorSet *updates,
                          std::function<void(decltype(updates))> receiver);

bool fix_null_descriptor_templates(
    device *dev, descriptor_update_template *updateTemplate, const void *pData,
    std::function<void(decltype(pData))> receiver);
