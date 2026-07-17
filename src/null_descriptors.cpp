#include "null_descriptors.hpp"

#include "descriptors.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "vk_func.hpp"
#include <cstring>
#include <deque>
#include <functional>
#include <variant>
#include <vulkan/vulkan.h>

namespace null_descriptor_fixer {

constexpr size_t get_stride(VkDescriptorType type) noexcept {
    switch (type) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return sizeof(VkDescriptorImageInfo);
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return sizeof(VkDescriptorBufferInfo);
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return sizeof(VkBufferView);
    default:
        return 0;
    }
}

inline bool needs_fixup(VkDescriptorType type, uint32_t count,
                        const void *src_data, size_t stride) noexcept {
    if (!src_data)
        return false;
    const auto *ptr =
        reinterpret_cast<const std::byte *>(src_data); // for custom stride

    const bool isSampler = type == VK_DESCRIPTOR_TYPE_SAMPLER ||
                           type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const bool isImageView = type != VK_DESCRIPTOR_TYPE_SAMPLER;

    for (int i = 0; i < count; i++) {
        const auto *current_elem = ptr + (i * stride);

        switch (type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
            if (reinterpret_cast<const VkDescriptorBufferInfo *>(current_elem)
                    ->buffer == VK_NULL_HANDLE)
                return true;
            break;
        }
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        case VK_DESCRIPTOR_TYPE_SAMPLER: {
            const auto *img =
                reinterpret_cast<const VkDescriptorImageInfo *>(current_elem);
            if ((isSampler && img->sampler == VK_NULL_HANDLE) ||
                (isImageView && img->imageView == VK_NULL_HANDLE)) {
                return true;
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            if (*reinterpret_cast<const VkBufferView *>(current_elem) ==
                VK_NULL_HANDLE)
                return true;
            break;
        }
        default:
            break;
        }
    }
    return false;
}

inline void fixup(const device *dev, VkDescriptorType type, uint32_t count,
                  void *dst_data, size_t stride) noexcept {
    if (!dst_data)
        return;
    auto *ptr = reinterpret_cast<std::byte *>(dst_data);

    const bool isSampler = type == VK_DESCRIPTOR_TYPE_SAMPLER ||
                           type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const bool isImageView = type != VK_DESCRIPTOR_TYPE_SAMPLER;

    for (int i = 0; i < count; i++) {
        auto *current_elem = ptr + (i * stride);

        switch (type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
            auto *buf =
                reinterpret_cast<VkDescriptorBufferInfo *>(current_elem);
            if (buf->buffer == VK_NULL_HANDLE) {
                buf->buffer = dev->null_descriptors.null_buffer;
                buf->offset = 0;
                buf->range = VK_WHOLE_SIZE;
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        case VK_DESCRIPTOR_TYPE_SAMPLER: {
            auto *img = reinterpret_cast<VkDescriptorImageInfo *>(current_elem);
            if (isImageView && img->imageView == VK_NULL_HANDLE) {
                img->imageView = dev->null_descriptors.null_image_view;
                img->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            }
            if (isSampler && img->sampler == VK_NULL_HANDLE) {
                img->sampler = dev->null_descriptors.null_sampler;
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            auto *view = reinterpret_cast<VkBufferView *>(current_elem);
            if (*view == VK_NULL_HANDLE) {
                *view = dev->null_descriptors.null_buffer_view;
            }
            break;
        }
        default:
            break;
        }
    }
}

} // namespace null_descriptor_fixer

using DescriptorInfoVariant =
    std::variant<std::vector<VkDescriptorBufferInfo>,
                 std::vector<VkDescriptorImageInfo>, std::vector<VkBufferView>>;

bool fix_null_descriptors(const device *dev, uint32_t updatesCount,
                          const VkWriteDescriptorSet *updates,
                          std::function<void(decltype(updates))> receiver) {
    std::vector<VkWriteDescriptorSet> patchedUpdates;
    std::deque<DescriptorInfoVariant> allocations; // deque is ptr-stable

    for (int i = 0; i < updatesCount; i++) {
        const auto &write = updates[i];
        const void *srcPtr = nullptr;
        if (write.pBufferInfo) {
            srcPtr = write.pBufferInfo;
        } else if (write.pImageInfo) {
            srcPtr = write.pImageInfo;
        } else if (write.pTexelBufferView) {
            srcPtr = write.pTexelBufferView;
        }

        const size_t stride =
            null_descriptor_fixer::get_stride(write.descriptorType);
        if (!srcPtr || stride == 0 ||
            !null_descriptor_fixer::needs_fixup(
                write.descriptorType, write.descriptorCount, srcPtr, stride)) {
            continue;
        }

        // Only do the potentially expensive copy if there's something to fixup
        if (patchedUpdates.empty()) {
            patchedUpdates.assign(updates, updates + updatesCount);
        }

        auto copy_descriptor_info = [&allocations](auto *ptr,
                                                   uint32_t count) -> auto & {
            using T = std::decay_t<decltype(*ptr)>;
            auto &item =
                allocations.emplace_back(std::vector<T>(ptr, ptr + count));
            return std::get<std::vector<T>>(item);
        };

        // Apply the nullDescriptor fixup patch to every descriptor write info
        const uint32_t n = write.descriptorCount;
        switch (write.descriptorType) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
            auto &bufferInfo = copy_descriptor_info(write.pBufferInfo, n);
            null_descriptor_fixer::fixup(dev, write.descriptorType, n,
                                         bufferInfo.data(), stride);
            patchedUpdates[i].pBufferInfo = bufferInfo.data();
            break;
        }
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        case VK_DESCRIPTOR_TYPE_SAMPLER: {
            auto &imageInfo = copy_descriptor_info(write.pImageInfo, n);
            null_descriptor_fixer::fixup(dev, write.descriptorType, n,
                                         imageInfo.data(), stride);
            patchedUpdates[i].pImageInfo = imageInfo.data();
            break;
        }
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            auto &bufferViews = copy_descriptor_info(write.pTexelBufferView, n);
            null_descriptor_fixer::fixup(dev, write.descriptorType, n,
                                         bufferViews.data(), stride);
            patchedUpdates[i].pTexelBufferView = bufferViews.data();
            break;
        }
        default:
            break;
        }
    }

    auto hasPatchedUpdates = patchedUpdates.empty();
    receiver(hasPatchedUpdates ? updates : patchedUpdates.data());
    return hasPatchedUpdates;
}

bool fix_null_descriptor_templates(
    device *dev, descriptor_update_template *updateTemplate, const void *pData,
    std::function<void(decltype(pData))> receiver) {

    size_t dataSize = 0;
    bool needsFixup = false;

    for (const auto &entry : updateTemplate->entries) {
        const size_t stride =
            null_descriptor_fixer::get_stride(entry.descriptorType);
        if (stride == 0)
            continue;

        const size_t entryEnd = entry.offset +
                                ((entry.descriptorCount - 1) * entry.stride) +
                                stride;
        if (entryEnd > dataSize) {
            dataSize = entryEnd;
        }

        if (!needsFixup) {
            const auto *srcPtr =
                reinterpret_cast<const std::byte *>(pData) + entry.offset;
            if (null_descriptor_fixer::needs_fixup(entry.descriptorType,
                                                   entry.descriptorCount,
                                                   srcPtr, entry.stride)) {
                needsFixup = true;
            }
        }
    }

    if (dataSize == 0 || !needsFixup) {
        receiver(pData);
        return false;
    }

    std::vector<uint8_t> patchedTemplateData(dataSize);
    std::memcpy(patchedTemplateData.data(), pData, dataSize);

    for (const auto &entry : updateTemplate->entries) {
        auto *dstPtr = patchedTemplateData.data() + entry.offset;
        null_descriptor_fixer::fixup(dev, entry.descriptorType,
                                     entry.descriptorCount, dstPtr,
                                     entry.stride);
    }

    receiver(patchedTemplateData.data());
    return true;
}

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

    if (result = DxvkMaliCompatLayer_CreateBuffer(
            device, &bufferCreateInfo, dev->alloc,
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

    result = DxvkMaliCompatLayer_BindBufferMemory(
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
