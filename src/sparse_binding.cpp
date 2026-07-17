#include "sparse_binding.hpp"

#include "buffer.hpp"
#include "command_buffer.hpp"
#include "image.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "queue.hpp"
#include "staging_resources.hpp"
#include "vk_func.hpp"

#include <algorithm>
#include <cstdlib>
#include <vulkan/vulkan.h>

namespace {

VkDeviceSize GetSparseBudget() {
    static VkDeviceSize budget = [] {
        if (const char *env = getenv("COMPAT_SPARSE_COMMIT_BUDGET")) {
            return static_cast<VkDeviceSize>(strtoull(env, nullptr, 10));
        }
        // Defaults to 8GB
        return static_cast<VkDeviceSize>(8 * 1024ull * 1024 * 1024);
    }();
    return budget;
}

bool ReserveBudget(struct device *dev, VkDeviceSize size) {
    VkDeviceSize budget = GetSparseBudget();
    VkDeviceSize before = dev->sparseCommittedBytes.fetch_add(size);
    if (before + size > budget) {
        dev->sparseCommittedBytes.fetch_sub(size);
        Logger::log("error",
                    "sparse_binding: commit of %llu bytes would exceed "
                    "budget (%llu already committed, budget %llu)",
                    (unsigned long long)size, (unsigned long long)before,
                    (unsigned long long)budget);
        return false;
    }
    return true;
}

void ReleaseBudget(struct device *dev, VkDeviceSize size) {
    dev->sparseCommittedBytes.fetch_sub(size);
}

void GetBlockDimensions(VkFormat format, uint32_t &blockWidth,
                        uint32_t &blockHeight) {
    blockWidth = 1;
    blockHeight = 1;

    switch (format) {
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT:
        blockWidth = 4;
        blockHeight = 4;
        return;
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT:
        blockWidth = 5;
        blockHeight = 4;
        return;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT:
        blockWidth = 5;
        blockHeight = 5;
        return;
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT:
        blockWidth = 6;
        blockHeight = 5;
        return;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT:
        blockWidth = 6;
        blockHeight = 6;
        return;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT:
        blockWidth = 8;
        blockHeight = 5;
        return;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT:
        blockWidth = 8;
        blockHeight = 6;
        return;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT:
        blockWidth = 8;
        blockHeight = 8;
        return;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT:
        blockWidth = 10;
        blockHeight = 5;
        return;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT:
        blockWidth = 10;
        blockHeight = 6;
        return;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT:
        blockWidth = 10;
        blockHeight = 8;
        return;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT:
        blockWidth = 10;
        blockHeight = 10;
        return;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT:
        blockWidth = 12;
        blockHeight = 10;
        return;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT:
        blockWidth = 12;
        blockHeight = 12;
        return;
    default:
        break;
    }

    if ((format >= VK_FORMAT_BC1_RGB_UNORM_BLOCK &&
         format <= VK_FORMAT_BC7_SRGB_BLOCK) ||
        (format >= VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK &&
         format <= VK_FORMAT_EAC_R11G11_SNORM_BLOCK)) {
        blockWidth = 4;
        blockHeight = 4;
    }
}

uint32_t GetTexelBytes(VkFormat format) {
    if ((format >= VK_FORMAT_ASTC_4x4_UNORM_BLOCK &&
         format <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK) ||
        (format >= VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT &&
         format <= VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT)) {
        return 16;
    }

    switch (format) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_S8_UINT:
        return 1;

    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_D16_UNORM:
        return 2;

    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return 4;

    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        return 8;

    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
        return 16;

    default:
        Logger::log("info",
                    "GetTexelBytes: format %d not found, "
                    "defaulting to 4-byte block shape",
                    static_cast<int>(format));
        return 4;
    }
}

} // namespace

VkExtent3D GetBlockShape(VkFormat format) {
    uint32_t elementBytes = GetTexelBytes(format);
    VkExtent3D blockShape = {128, 128, 1};

    switch (elementBytes) {
    case 1:
        blockShape = {256, 256, 1};
        break;
    case 2:
        blockShape = {256, 128, 1};
        break;
    case 4:
        blockShape = {128, 128, 1};
        break;
    case 8:
        blockShape = {128, 64, 1};
        break;
    case 16:
        blockShape = {64, 64, 1};
        break;
    default:
        break;
    }

    uint32_t blockWidth, blockHeight;
    GetBlockDimensions(format, blockWidth, blockHeight);

    blockShape.width *= blockWidth;
    blockShape.height *= blockHeight;

    return blockShape;
}

VkResult BindSparseBuffer(struct device *dev,
                          const VkBufferCreateInfo *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator,
                          buffer *buf) {
    VkMemoryRequirements reqs;
    dev->table.GetBufferMemoryRequirements(dev->handle, buf->handle, &reqs);

    if (!ReserveBudget(dev, reqs.size)) {
        dev->table.DestroyBuffer(dev->handle, buf->handle, pAllocator);
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = FindMemoryType(dev, reqs.memoryTypeBits),
    };
    VkDeviceMemory shadowMemory;
    VkResult result = dev->table.AllocateMemory(dev->handle, &allocInfo,
                                                pAllocator, &shadowMemory);
    if (result != VK_SUCCESS) {
        ReleaseBudget(dev, reqs.size);
        dev->table.DestroyBuffer(dev->handle, buf->handle, pAllocator);
        Logger::log("error", "sparse_binding: shadow allocation failed: %d",
                    result);
        return result;
    }
    DxvkMaliCompatLayer_BindBufferMemory(dev->handle, buf->handle, shadowMemory,
                                         0);

    auto res = std::make_unique<dense_sparse_resource>();
    res->kind = sparse_resource_kind::buffer;
    res->buffer = buf->handle;
    res->shadowMemory = shadowMemory;
    res->virtualSize = pCreateInfo->size;
    res->pageGranularity = reqs.alignment;
    res->memoryTypeIndex = allocInfo.memoryTypeIndex;
    res->device = dev;
    res->alloc = pAllocator;

    buf->sparse_resource = std::move(res);

    Logger::log("info",
                "sparse_binding: emulated sparse buffer, virtualSize=%llu "
                "committed=%llu",
                (unsigned long long)pCreateInfo->size,
                (unsigned long long)reqs.size);
    return VK_SUCCESS;
}

VkResult BindSparseImage(struct device *dev,
                         const VkImageCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator, image *img) {
    VkMemoryRequirements reqs;
    dev->table.GetImageMemoryRequirements(dev->handle, img->handle, &reqs);

    if (!ReserveBudget(dev, reqs.size)) {
        dev->table.DestroyImage(dev->handle, img->handle, pAllocator);
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = FindMemoryType(dev, reqs.memoryTypeBits),
    };
    VkDeviceMemory shadowMemory;
    VkResult result = dev->table.AllocateMemory(dev->handle, &allocInfo,
                                                pAllocator, &shadowMemory);
    if (result != VK_SUCCESS) {
        ReleaseBudget(dev, reqs.size);
        dev->table.DestroyImage(dev->handle, img->handle, pAllocator);
        Logger::log("error", "sparse_binding: shadow allocation failed: %d",
                    result);
        return result;
    }
    dev->table.BindImageMemory(dev->handle, img->handle, shadowMemory, 0);

    auto res = std::make_unique<dense_sparse_resource>();
    res->kind = sparse_resource_kind::image;
    res->image = img->handle;
    res->shadowMemory = shadowMemory;
    res->virtualSize = reqs.size;
    res->imageAspect =
        (pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            ? VK_IMAGE_ASPECT_DEPTH_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT;
    res->imageGranularity = GetBlockShape(pCreateInfo->format);
    res->memoryTypeIndex = allocInfo.memoryTypeIndex;
    res->device = dev;
    res->alloc = pAllocator;
    res->imageResident = false;
    img->sparse_resource = std::move(res);

    Logger::log("info",
                "sparse_binding: emulated sparse image, committed=%llu, "
                "blockShape=%ux%ux%u",
                (unsigned long long)reqs.size,
                res.get() ? res->imageGranularity.width : 0, 0u, 0u);
    return VK_SUCCESS;
}

bool DestroySparseBuffer(struct device *dev, VkBuffer buffer,
                         const VkAllocationCallbacks *pAllocator) {
    auto res = find_sparse_buffer(buffer);
    if (!res)
        return false;

    dev->table.FreeMemory(dev->handle, res->shadowMemory, pAllocator);
    ReleaseBudget(dev, res->virtualSize);
    return true;
}

bool DestroySparseImage(struct device *dev, VkImage image,
                        const VkAllocationCallbacks *pAllocator) {
    auto res = find_sparse_image(image);
    if (!res)
        return false;

    dev->table.FreeMemory(dev->handle, res->shadowMemory, pAllocator);
    ReleaseBudget(dev, res->virtualSize);
    return true;
}

void GetSparseBufferMemoryRequirements(const dense_sparse_resource *res,
                                       VkMemoryRequirements *pRequirements) {
    if (!res)
        return;

    auto align = [](VkDeviceSize v, VkDeviceSize a) {
        return (v + a - 1) & ~(a - 1);
    };

    pRequirements->size = align(res->virtualSize, res->pageGranularity);
    pRequirements->alignment = res->pageGranularity;
    pRequirements->memoryTypeBits = 1u << res->memoryTypeIndex;
}

void GetSparseImageMemoryRequirements(
    const dense_sparse_resource *res, uint32_t *pCount,
    VkSparseImageMemoryRequirements *pRequirements) {
    if (!res)
        return;

    if (!pRequirements) {
        *pCount = 1;
        return;
    }
    *pCount = std::min(*pCount, 1u);
    if (*pCount == 0)
        return;

    VkSparseImageMemoryRequirements &r = pRequirements[0];
    r.formatProperties.aspectMask = res->imageAspect;
    r.formatProperties.imageGranularity = res->imageGranularity;
    r.formatProperties.flags = VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;
    r.imageMipTailFirstLod = 0;
    r.imageMipTailSize = res->virtualSize;
    r.imageMipTailOffset = 0;
    r.imageMipTailStride = 0;
}

namespace {

void RecordClearResource(struct device *dev, VkCommandBuffer cb,
                         dense_sparse_resource *res, VkDeviceSize offset,
                         VkDeviceSize size) {
    if (res->kind == sparse_resource_kind::buffer) {
        dev->table.CmdFillBuffer(cb, res->buffer, offset, size, 0);
    } else {
        VkImageSubresourceRange range{res->imageAspect, 0,
                                      VK_REMAINING_MIP_LEVELS, 0,
                                      VK_REMAINING_ARRAY_LAYERS};
        if (res->imageAspect & VK_IMAGE_ASPECT_COLOR_BIT) {
            VkClearColorValue clear{};
            dev->table.CmdClearColorImage(
                cb, res->image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
        } else {
            VkClearDepthStencilValue clear{0.0f, 0};
            dev->table.CmdClearDepthStencilImage(
                cb, res->image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
        }
    }
}

void RecordBufferBinds(struct queue *q, struct command_buffer *cb,
                       const VkSparseBufferMemoryBindInfo &info) {
    struct device *dev = q->device;
    dense_sparse_resource *res = find_sparse_buffer(info.buffer);
    if (!res) {
        Logger::log("error",
                    "sparse_binding: vkQueueBindSparse on untracked buffer");
        return;
    }

    for (uint32_t i = 0; i < info.bindCount; i++) {
        const VkSparseMemoryBind &bind = info.pBinds[i];

        if (bind.flags & VK_SPARSE_MEMORY_BIND_METADATA_BIT) {
            continue;
        }

        if (bind.memory == VK_NULL_HANDLE) {
            RecordClearResource(dev, cb->handle, res, bind.resourceOffset,
                                bind.size);
        } else {
            auto *srcBuffer = CreateStagingBuffer(
                dev, bind.size, "sparse_staging_buffer",
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bind.memory);
            if (srcBuffer == nullptr) {
                Logger::log("error",
                            "sparse_binding: failed to create staging buffer");
                continue;
            }
            VkBufferCopy copy{0, bind.resourceOffset, bind.size};
            dev->table.CmdCopyBuffer(cb->handle, srcBuffer->handle, res->buffer,
                                     1, &copy);

            if (cb->currentStagingResources) {
                cb->currentStagingResources->AddStagingBuffer(
                    srcBuffer->handle);
            }
        }
    }
}

void RecordImageOpaqueBinds(struct queue *q, struct command_buffer *cb,
                            const VkSparseImageOpaqueMemoryBindInfo &info) {
    struct device *dev = q->device;
    dense_sparse_resource *res = find_sparse_image(info.image);
    if (!res) {
        Logger::log("error",
                    "sparse_binding: vkQueueBindSparse on untracked image");
        return;
    }

    for (uint32_t i = 0; i < info.bindCount; i++) {
        const VkSparseMemoryBind &bind = info.pBinds[i];
        if (bind.flags & VK_SPARSE_MEMORY_BIND_METADATA_BIT)
            continue;

        res->imageResident = bind.memory != VK_NULL_HANDLE;
        if (!res->imageResident) {
            RecordClearResource(dev, cb->handle, res, 0, res->virtualSize);
        }
    }
}

void RecordImageGranularBinds(struct queue *q, struct command_buffer *cb,
                              const VkSparseImageMemoryBindInfo &info) {
    static bool hasLogged = false;
    if (!hasLogged) {
        Logger::log("info", "WARN: vkQueueBindSparse with granular binds are "
                            "no-op under emulation");
        hasLogged = true;
    }
}

} // namespace

VkResult EmulatevkQueueBindSparse(struct queue *q, uint32_t bindInfoCount,
                                  const VkBindSparseInfo *pBindInfo,
                                  VkFence fence) {
    struct device *dev = q->device;
    const auto &table = dev->table;

    VkSemaphore prev_semaphore = VK_NULL_HANDLE;
    std::vector<VkSemaphore> stagingSemaphores;

    for (uint32_t b = 0; b < bindInfoCount; b++) {
        const VkBindSparseInfo &info = pBindInfo[b];

        auto finalBind = b == bindInfoCount - 1;
        VkFence signalFence = finalBind ? fence : VK_NULL_HANDLE;

        std::vector<VkSemaphore> waits(info.pWaitSemaphores,
                                       info.pWaitSemaphores +
                                           info.waitSemaphoreCount);
        std::vector<VkSemaphore> signals(info.pSignalSemaphores,
                                         info.pSignalSemaphores +
                                             info.signalSemaphoreCount);

        if (prev_semaphore != VK_NULL_HANDLE) {
            waits.push_back(prev_semaphore);
        }

        // TODO: create a binary semaphore pool
        VkSemaphore next_semaphore = VK_NULL_HANDLE;
        if (b < bindInfoCount - 1) {
            VkSemaphoreCreateInfo sem_info{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            VkResult res = table.CreateSemaphore(dev->handle, &sem_info,
                                                 dev->alloc, &next_semaphore);
            if (res != VK_SUCCESS) {
                Logger::log("error",
                            "sparse_binding: failed to create intermediate "
                            "semaphore: %d",
                            res);
                return res;
            }
            signals.push_back(next_semaphore);
            stagingSemaphores.push_back(next_semaphore);
        }

        VkResult result = SubmitOneShotAsync(
            dev,
            [&](struct command_buffer *cb) {
                if (finalBind) {
                    for (auto stagingSemaphore : stagingSemaphores) {
                        cb->currentStagingResources->AddStagingSemaphore(
                            stagingSemaphore);
                    }
                }
                {
                    cb->currentStagingResources->MakeScopedTimestampQuery(
                        cb, "emulate_all_bind_sparse");
                    if (info.bufferBindCount > 0) {
                        cb->currentStagingResources->MakeScopedTimestampQuery(
                            cb, "emulate_buffer_binds_sparse");
                        for (int i = 0; i < info.bufferBindCount; i++) {
                            RecordBufferBinds(q, cb, info.pBufferBinds[i]);
                        }
                    }

                    if (info.imageOpaqueBindCount > 0 ||
                        info.imageBindCount > 0) {
                        cb->currentStagingResources->MakeScopedTimestampQuery(
                            cb, "emulate_image_binds_sparse");
                        for (int i = 0; i < info.imageOpaqueBindCount; i++) {
                            RecordImageOpaqueBinds(q, cb,
                                                   info.pImageOpaqueBinds[i]);
                        }

                        for (int i = 0; i < info.imageBindCount; i++) {
                            RecordImageGranularBinds(q, cb,
                                                     info.pImageBinds[i]);
                        }
                    }
                }
            },
            "sparse_binding_bind_op", q, waits, signals, signalFence);

        if (result != VK_SUCCESS) {
            Logger::log("error",
                        "sparse_binding: SubmitOneShotAsync failed: %d",
                        result);
            return result;
        }

        prev_semaphore = next_semaphore;
    }

    return VK_SUCCESS;
}
