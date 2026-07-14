#include "null_descriptors.hpp"

#include "layer.hpp"
#include "logger.hpp"
#include <cstring>
#include <vulkan/vulkan.h>

void create_null_resources(struct device *dev) {
    const auto &table = dev->table;
    auto device = dev->handle;
    VkResult result;

    VkPhysicalDeviceMemoryProperties memoryProps{};
    instanceDispatch[GetInstanceKey(dev->physical)]
        .GetPhysicalDeviceMemoryProperties(dev->physical, &memoryProps);

    VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = 1024 * 1024, // rely on robustBufferAccess
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (result = dev->table.CreateBuffer_(device, &bufferCreateInfo, dev->alloc,
                                          &dev->null_descriptors.null_buffer);
        result != VK_SUCCESS) {
        Logger::log("error", "Failed to create null_buffer, result: %d",
                    result);
        return;
    }

    VkMemoryRequirements memoryReqs;
    table.GetBufferMemoryRequirements(device, dev->null_descriptors.null_buffer,
                                      &memoryReqs);
    uint32_t memoryType = UINT32_MAX;
    for (uint32_t i = 0; i < memoryProps.memoryTypeCount; i++) {
        if ((memoryReqs.memoryTypeBits & (1u << i)) &&
            (memoryProps.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            memoryType = i;
            break;
        }
    }

    // TODO: fall back to using a non-host_visible memory type and skip the
    // vkMapMemory below
    if (memoryType == UINT32_MAX) {
        Logger::log("error",
                    "Failed to find suitable memory type for null_buffer "
                    "(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)");
        return;
    }

    VkMemoryAllocateInfo memoryAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryReqs.size,
        .memoryTypeIndex = memoryType,
    };
    if (result =
            table.AllocateMemory(device, &memoryAllocInfo, dev->alloc,
                                 &dev->null_descriptors.null_buffer_memory);
        result != VK_SUCCESS) {
        Logger::log("error",
                    "Failed to allocate memory for null_buffer, result: %d",
                    result);
        return;
    }

    result = dev->table.BindBufferMemory(
        device, dev->null_descriptors.null_buffer,
        dev->null_descriptors.null_buffer_memory, 0);
    if (result != VK_SUCCESS) {
        Logger::log("error",
                    "Failed to bind memory for null_buffer, result: %d",
                    result);
        return;
    }

    void *mappedData = nullptr;
    if (table.MapMemory(device, dev->null_descriptors.null_buffer_memory, 0,
                        VK_WHOLE_SIZE, 0, &mappedData) == VK_SUCCESS) {
        std::memset(mappedData, 0, memoryReqs.size);
        VkMappedMemoryRange range = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = dev->null_descriptors.null_buffer_memory,
            .offset = 0,
            .size = VK_WHOLE_SIZE};
        table.FlushMappedMemoryRanges(device, 1, &range);
        table.UnmapMemory(device, dev->null_descriptors.null_buffer_memory);
    }

    // TODO(leegao): support other formats as well
    VkBufferViewCreateInfo bufferViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .buffer = dev->null_descriptors.null_buffer,
        .format = VK_FORMAT_R32_UINT,
        .offset = 0,
        .range = VK_WHOLE_SIZE};
    if (table.CreateBufferView(device, &bufferViewCreateInfo, dev->alloc,
                               &dev->null_descriptors.null_buffer_view) !=
        VK_SUCCESS) {
        Logger::log("error", "Failed to create null_buffer_view");
    }

    // TODO(leegao): support 3D texture types and other formats as well
    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1, 1, 1}, // rely on VK_EXT_image_robustness
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                 VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (table.CreateImage(device, &imageCreateInfo, dev->alloc,
                          &dev->null_descriptors.null_image) == VK_SUCCESS) {
        VkMemoryRequirements imageMemoryReqs;
        table.GetImageMemoryRequirements(
            device, dev->null_descriptors.null_image, &imageMemoryReqs);
        uint32_t imageMemoryType = UINT32_MAX;
        for (uint32_t i = 0; i < memoryProps.memoryTypeCount; i++) {
            if (imageMemoryReqs.memoryTypeBits & (1u << i)) {
                imageMemoryType = i;
                break;
            }
        }
        VkMemoryAllocateInfo imageMemoryAllocationInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = imageMemoryReqs.size,
            .memoryTypeIndex = imageMemoryType,
        };
        if (imageMemoryType != UINT32_MAX &&
            table.AllocateMemory(device, &imageMemoryAllocationInfo, dev->alloc,
                                 &dev->null_descriptors.null_image_memory) ==
                VK_SUCCESS) {
            table.BindImageMemory(device, dev->null_descriptors.null_image,
                                  dev->null_descriptors.null_image_memory, 0);
            VkImageViewCreateInfo imageViewCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = dev->null_descriptors.null_image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            };
            table.CreateImageView(device, &imageViewCreateInfo, dev->alloc,
                                  &dev->null_descriptors.null_image_view);
        }
    }

    VkSamplerCreateInfo samplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    };
    table.CreateSampler(device, &samplerCreateInfo, dev->alloc,
                        &dev->null_descriptors.null_sampler);
}

void destroy_null_resources(struct device *dev) {
    const auto &table = dev->table;
    VkDevice handle = dev->handle;

    if (dev->null_descriptors.null_sampler != VK_NULL_HANDLE)
        table.DestroySampler(handle, dev->null_descriptors.null_sampler,
                             dev->alloc);
    if (dev->null_descriptors.null_image_view != VK_NULL_HANDLE)
        table.DestroyImageView(handle, dev->null_descriptors.null_image_view,
                               dev->alloc);
    if (dev->null_descriptors.null_image != VK_NULL_HANDLE)
        table.DestroyImage(handle, dev->null_descriptors.null_image,
                           dev->alloc);
    if (dev->null_descriptors.null_image_memory != VK_NULL_HANDLE)
        table.FreeMemory(handle, dev->null_descriptors.null_image_memory,
                         dev->alloc);
    if (dev->null_descriptors.null_buffer_view != VK_NULL_HANDLE)
        table.DestroyBufferView(handle, dev->null_descriptors.null_buffer_view,
                                dev->alloc);
    if (dev->null_descriptors.null_buffer != VK_NULL_HANDLE)
        table.DestroyBuffer(handle, dev->null_descriptors.null_buffer,
                            dev->alloc);
    if (dev->null_descriptors.null_buffer_memory != VK_NULL_HANDLE)
        table.FreeMemory(handle, dev->null_descriptors.null_buffer_memory,
                         dev->alloc);
}
