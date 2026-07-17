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
#include <vulkan/vulkan.h>

VkExtent3D GetBlockShape(VkFormat format) { return {128, 128, 1}; }

VkResult BindSparseBuffer(struct device *dev,
                          const VkBufferCreateInfo *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator,
                          buffer *buf) {
    VkMemoryRequirements reqs;
    dev->table.GetBufferMemoryRequirements(dev->handle, buf->handle, &reqs);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = FindMemoryType(dev, reqs.memoryTypeBits),
    };
    VkDeviceMemory shadowMemory;
    VkResult result = dev->table.AllocateMemory(dev->handle, &allocInfo,
                                                pAllocator, &shadowMemory);
    if (result != VK_SUCCESS) {
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

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = FindMemoryType(dev, reqs.memoryTypeBits),
    };
    VkDeviceMemory shadowMemory;
    VkResult result = dev->table.AllocateMemory(dev->handle, &allocInfo,
                                                pAllocator, &shadowMemory);
    if (result != VK_SUCCESS) {
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
    return true;
}

bool DestroySparseImage(struct device *dev, VkImage image,
                        const VkAllocationCallbacks *pAllocator) {
    auto res = find_sparse_image(image);
    if (!res)
        return false;

    dev->table.FreeMemory(dev->handle, res->shadowMemory, pAllocator);
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
