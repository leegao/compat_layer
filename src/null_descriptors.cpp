#include "null_descriptors.hpp"

#include "buffer.hpp"
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

inline void fixup(device *dev, VkDescriptorType type, uint32_t count,
                  void *dst_data, size_t stride, VkFormat texelFormatHint,
                  VkImageViewType imageViewTypeHint) noexcept {
    if (!dst_data)
        return;
    auto *ptr = reinterpret_cast<std::byte *>(dst_data);

    const bool isSampler = type == VK_DESCRIPTOR_TYPE_SAMPLER ||
                           type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    const bool isImageView = type != VK_DESCRIPTOR_TYPE_SAMPLER;

    const VkImageViewType effectiveViewType =
        (type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT &&
         imageViewTypeHint == VK_IMAGE_VIEW_TYPE_MAX_ENUM)
            ? VK_IMAGE_VIEW_TYPE_2D
            : imageViewTypeHint;

    VkImageView imageView = VK_NULL_HANDLE;
    VkBufferView bufferView = VK_NULL_HANDLE;
    if (isImageView) {
        imageView = get_null_image_view(dev, effectiveViewType, texelFormatHint,
                                        VK_SAMPLE_COUNT_1_BIT);
    }
    if (type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
        type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) {
        bufferView = get_null_buffer_view(dev, texelFormatHint);
    }

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
                img->imageView = imageView;
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
                *view = bufferView;
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

bool fix_null_descriptors(struct device *dev, uint32_t updatesCount,
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

        VkFormat formatHint = VK_FORMAT_UNDEFINED;
        VkImageViewType viewTypeHint = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

        auto layoutHandle = get_layout_for_set(write.dstSet);
        if (dev->emulate_precise_null_descriptor &&
            layoutHandle != VK_NULL_HANDLE) {
            auto *descriptorSetLayout = ({
                std::shared_lock l(descriptorSetLayoutsLock);
                get_descriptor_set_layout(layoutHandle);
            });
            if (descriptorSetLayout) {
                std::shared_lock l_hints(descriptorSetLayout->hintsLock);
                auto it =
                    descriptorSetLayout->bindingHints.find(write.dstBinding);
                if (it != descriptorSetLayout->bindingHints.end()) {
                    formatHint = it->second.format;
                    viewTypeHint = it->second.imageViewType;
                }
            }
        }

        // Apply the nullDescriptor fixup patch to every descriptor write info
        const uint32_t n = write.descriptorCount;
        switch (write.descriptorType) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
            auto &bufferInfo = copy_descriptor_info(write.pBufferInfo, n);
            null_descriptor_fixer::fixup(dev, write.descriptorType, n,
                                         bufferInfo.data(), stride, formatHint,
                                         viewTypeHint);
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
                                         imageInfo.data(), stride, formatHint,
                                         viewTypeHint);
            patchedUpdates[i].pImageInfo = imageInfo.data();
            break;
        }
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            auto &bufferViews = copy_descriptor_info(write.pTexelBufferView, n);
            null_descriptor_fixer::fixup(dev, write.descriptorType, n,
                                         bufferViews.data(), stride, formatHint,
                                         viewTypeHint);
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

        VkFormat formatHint = VK_FORMAT_UNDEFINED;
        VkImageViewType viewTypeHint = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

        if (dev->emulate_precise_null_descriptor &&
            updateTemplate->layout != VK_NULL_HANDLE) {
            auto *descriptorSetLayout = ({
                std::shared_lock l(descriptorSetLayoutsLock);
                get_descriptor_set_layout(updateTemplate->layout);
            });
            if (descriptorSetLayout) {
                std::shared_lock l_hints(descriptorSetLayout->hintsLock);
                auto it =
                    descriptorSetLayout->bindingHints.find(entry.dstBinding);
                if (it != descriptorSetLayout->bindingHints.end()) {
                    formatHint = it->second.format;
                    viewTypeHint = it->second.imageViewType;
                }
            }
        }

        null_descriptor_fixer::fixup(dev, entry.descriptorType,
                                     entry.descriptorCount, dstPtr,
                                     entry.stride, formatHint, viewTypeHint);
    }

    receiver(patchedTemplateData.data());
    return true;
}

namespace {

VkBufferView CreateNullBufferView(struct device *dev, VkFormat format) {
    VkBufferViewCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .buffer = dev->null_descriptors.null_buffer,
        .format = format,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkBufferView view = VK_NULL_HANDLE;
    VkResult result =
        dev->table.CreateBufferView(dev->handle, &info, dev->alloc, &view);
    if (result != VK_SUCCESS) {
        Logger::log(
            "error",
            "Failed to create null_buffer_view for format %d, result: %d",
            static_cast<int>(format), result);
        return VK_NULL_HANDLE;
    }
    return view;
}

null_image_resource CreateNullImageResource(struct device *dev,
                                            VkImageViewType viewType,
                                            VkFormat format,
                                            VkSampleCountFlagBits samples) {
    const auto &table = dev->table;
    null_image_resource resource;

    auto memoryProps = dev->memoryProps;

    VkImageType imageType;
    uint32_t arrayLayers = 1;
    VkImageCreateFlags createFlags = 0;
    switch (viewType) {
    case VK_IMAGE_VIEW_TYPE_1D:
        imageType = VK_IMAGE_TYPE_1D;
        break;
    case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        imageType = VK_IMAGE_TYPE_1D;
        arrayLayers = 1;
        break;
    case VK_IMAGE_VIEW_TYPE_3D:
        imageType = VK_IMAGE_TYPE_3D;
        break;
    case VK_IMAGE_VIEW_TYPE_CUBE:
        imageType = VK_IMAGE_TYPE_2D;
        arrayLayers = 6;
        createFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        imageType = VK_IMAGE_TYPE_2D;
        arrayLayers = 6;
        createFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        imageType = VK_IMAGE_TYPE_2D;
        arrayLayers = 1;
        break;
    case VK_IMAGE_VIEW_TYPE_2D:
    default:
        imageType = VK_IMAGE_TYPE_2D;
        break;
    }

    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = createFlags,
        .imageType = imageType,
        .format = format,
        .extent = {1, 1, imageType == VK_IMAGE_TYPE_3D ? 1u : 1u},
        .mipLevels = 1,
        .arrayLayers = arrayLayers,
        .samples = samples,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                 VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkResult result = DxvkMaliCompatLayer_CreateImage(
        dev->handle, &imageCreateInfo, dev->alloc, &resource.image);
    if (result != VK_SUCCESS) {
        Logger::log("error",
                    "Failed to create null image (viewType=%d, format=%d, "
                    "samples=%d): %d",
                    static_cast<int>(viewType), static_cast<int>(format),
                    static_cast<int>(samples), result);
        return resource;
    }

    VkMemoryRequirements memReqs;
    table.GetImageMemoryRequirements(dev->handle, resource.image, &memReqs);
    auto memoryType = FindMemoryType(dev, memReqs.memoryTypeBits);
    if (memoryType == UINT32_MAX) {
        Logger::log("error", "Failed to find memory type for null image");
        DxvkMaliCompatLayer_DestroyImage(dev->handle, resource.image,
                                         dev->alloc);
        resource.image = VK_NULL_HANDLE;
        return resource;
    }

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memoryType,
    };
    result = table.AllocateMemory(dev->handle, &allocInfo, dev->alloc,
                                  &resource.memory);
    if (result != VK_SUCCESS) {
        Logger::log("error",
                    "Failed to allocate memory for null image, result: %d",
                    result);
        DxvkMaliCompatLayer_DestroyImage(dev->handle, resource.image,
                                         dev->alloc);
        resource.image = VK_NULL_HANDLE;
        return resource;
    }
    table.BindImageMemory(dev->handle, resource.image, resource.memory, 0);

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = resource.image,
        .viewType = viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM
                        ? VK_IMAGE_VIEW_TYPE_2D
                        : viewType,
        .format = format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, arrayLayers},
    };
    result = DxvkMaliCompatLayer_CreateImageView(dev->handle, &viewInfo,
                                                 dev->alloc, &resource.view);
    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to create null image view, result: %d",
                    result);
        table.FreeMemory(dev->handle, resource.memory, dev->alloc);
        DxvkMaliCompatLayer_DestroyImage(dev->handle, resource.image,
                                         dev->alloc);
        resource.image = VK_NULL_HANDLE;
        resource.memory = VK_NULL_HANDLE;
    }
    return resource;
}

} // namespace

VkBufferView get_null_buffer_view(struct device *dev, VkFormat format) {
    auto &nullDescriptors = dev->null_descriptors;
    if (format == VK_FORMAT_UNDEFINED || format == VK_FORMAT_R32_UINT) {
        return nullDescriptors.null_buffer_view;
    }

    {
        std::shared_lock l(nullDescriptors.cacheLock);
        auto it = nullDescriptors.bufferViewsByFormat.find(format);
        if (it != nullDescriptors.bufferViewsByFormat.end())
            return it->second;
    }

    VkBufferView view = CreateNullBufferView(dev, format);
    std::unique_lock l(nullDescriptors.cacheLock); // writer
    nullDescriptors.bufferViewsByFormat.emplace(format, view);
    return view;
}

VkImageView get_null_image_view(struct device *dev, VkImageViewType viewType,
                                VkFormat format,
                                VkSampleCountFlagBits samples) {
    auto &nullDescriptor = dev->null_descriptors;
    const bool isDefaultShape =
        (viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM ||
         viewType == VK_IMAGE_VIEW_TYPE_2D) &&
        (format == VK_FORMAT_UNDEFINED || format == VK_FORMAT_R8G8B8A8_UNORM) &&
        samples == VK_SAMPLE_COUNT_1_BIT;
    if (isDefaultShape) {
        return nullDescriptor.null_image_rgba8_2d.view;
    }

    const VkFormat resolvedFormat =
        format == VK_FORMAT_UNDEFINED ? VK_FORMAT_R8G8B8A8_UNORM : format;
    const VkImageViewType resolvedViewType =
        viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM ? VK_IMAGE_VIEW_TYPE_2D
                                                : viewType;
    auto key = GetImageShapeKey(resolvedViewType, resolvedFormat, samples);
    {
        std::shared_lock l(nullDescriptor.cacheLock);
        auto it = nullDescriptor.imagesByShape.find(key);
        if (it != nullDescriptor.imagesByShape.end())
            return it->second.view;
    }

    auto nullImage =
        CreateNullImageResource(dev, resolvedViewType, resolvedFormat, samples);
    auto imageView = nullImage.view;
    std::unique_lock l(nullDescriptor.cacheLock); // writer
    nullDescriptor.imagesByShape.emplace(key, std::move(nullImage));
    return imageView;
}

void create_null_resources(struct device *dev) {
    const auto &table = dev->table;
    auto device = dev->handle;
    VkResult result;

    const auto &memoryProps = dev->memoryProps;
    VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = 1024 * 1024, // rely on robustBufferAccess
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
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
    auto memoryType = FindMemoryType(dev, memoryReqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
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
    dev->null_descriptors.null_buffer_view =
        CreateNullBufferView(dev, VK_FORMAT_R32_UINT);

    dev->null_descriptors.null_image_rgba8_2d = CreateNullImageResource(
        dev, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
        VK_SAMPLE_COUNT_1_BIT);

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
    auto &nullDescriptor = dev->null_descriptors;

    if (nullDescriptor.null_sampler != VK_NULL_HANDLE)
        table.DestroySampler(handle, nullDescriptor.null_sampler, dev->alloc);

    for (auto &[key, variant] : nullDescriptor.imagesByShape) {
        if (variant.view != VK_NULL_HANDLE)
            table.DestroyImageView(handle, variant.view, dev->alloc);
        if (variant.image != VK_NULL_HANDLE)
            DxvkMaliCompatLayer_DestroyImage(handle, variant.image, dev->alloc);
        if (variant.memory != VK_NULL_HANDLE)
            table.FreeMemory(handle, variant.memory, dev->alloc);
    }
    nullDescriptor.imagesByShape.clear();

    for (auto &[format, view] : nullDescriptor.bufferViewsByFormat) {
        if (view != VK_NULL_HANDLE)
            table.DestroyBufferView(handle, view, dev->alloc);
    }
    nullDescriptor.bufferViewsByFormat.clear();

    if (nullDescriptor.null_image_rgba8_2d.view != VK_NULL_HANDLE)
        table.DestroyImageView(handle, nullDescriptor.null_image_rgba8_2d.view,
                               dev->alloc);
    if (nullDescriptor.null_image_rgba8_2d.image != VK_NULL_HANDLE)
        DxvkMaliCompatLayer_DestroyImage(
            handle, nullDescriptor.null_image_rgba8_2d.image, dev->alloc);
    if (nullDescriptor.null_image_rgba8_2d.memory != VK_NULL_HANDLE)
        table.FreeMemory(handle, nullDescriptor.null_image_rgba8_2d.memory,
                         dev->alloc);
    if (nullDescriptor.null_buffer_view != VK_NULL_HANDLE)
        table.DestroyBufferView(handle, nullDescriptor.null_buffer_view,
                                dev->alloc);
    if (nullDescriptor.null_buffer != VK_NULL_HANDLE)
        DxvkMaliCompatLayer_DestroyBuffer(handle, nullDescriptor.null_buffer,
                                          dev->alloc);
    if (nullDescriptor.null_buffer_memory != VK_NULL_HANDLE)
        table.FreeMemory(handle, nullDescriptor.null_buffer_memory, dev->alloc);
}
