#pragma once

#include <cstdint>
#include <string_view>
#include <vulkan/vulkan.h>

struct device;
struct queue;

enum class sparse_resource_kind { buffer, image };

// Always resident "sparse" resource shadows to emulate sparseBinding
struct dense_sparse_resource {
    sparse_resource_kind kind;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory shadowMemory = VK_NULL_HANDLE;

    VkDeviceSize virtualSize = 0; // app-declared size (buffer) or
                                  // aggregate byte size (image)
    VkDeviceSize pageGranularity = 0;
    VkExtent3D imageGranularity{};
    VkImageAspectFlags imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t memoryTypeIndex = 0;
    struct device *device = nullptr;
    const VkAllocationCallbacks *alloc = nullptr;

    bool imageResident = false;

    std::string_view label;
};

VkResult BindSparseBuffer(struct device *dev,
                          const VkBufferCreateInfo *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator,
                          struct buffer *buf);

VkResult BindSparseImage(struct device *dev,
                         const VkImageCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         struct image *img);

bool DestroySparseBuffer(struct device *dev, VkBuffer buffer,
                         const VkAllocationCallbacks *pAllocator);
bool DestroySparseImage(struct device *dev, VkImage image,
                        const VkAllocationCallbacks *pAllocator);

void GetSparseBufferMemoryRequirements(const dense_sparse_resource *res,
                                       VkMemoryRequirements *pRequirements);

void GetSparseImageMemoryRequirements(
    const dense_sparse_resource *res, uint32_t *pCount,
    VkSparseImageMemoryRequirements *pRequirements);

VkResult EmulatevkQueueBindSparse(struct queue *q, uint32_t bindInfoCount,
                                  const VkBindSparseInfo *pBindInfo,
                                  VkFence fence);

VkExtent3D GetBlockShape(VkFormat format);
