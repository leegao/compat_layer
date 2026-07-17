#include "image.hpp"
#include "logger.hpp"
#include "sparse_binding.hpp"
#include <vulkan/vulkan.h>

std::unordered_map<VkImage, std::unique_ptr<struct image>> imagesMap;

struct image *find_image(VkImage image) {
    auto it = imagesMap.find(image);
    if (it == imagesMap.end())
        return nullptr;
    return it->second.get();
}

struct dense_sparse_resource *find_sparse_image(VkImage image) {
    auto img = find_image(image);
    return img ? img->sparse_resource.get() : nullptr;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateImage(
    VkDevice device, const VkImageCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkImage *pImage) {
    VkResult result;

    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_UNKNOWN;

    static constexpr VkImageCreateFlags kSparseFlags =
        VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
        VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT |
        VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;

    VkImageCreateInfo create_info = *pCreateInfo;
    auto emulate_sparse_binding =
        dev->emulate_sparse_binding && (pCreateInfo->flags & kSparseFlags);
    if (emulate_sparse_binding) {
        create_info.flags &= ~(VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                               VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT |
                               VK_IMAGE_CREATE_SPARSE_ALIASED_BIT);
    }

    result = dev->table.CreateImage(device, &create_info, pAllocator, pImage);
    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to create image, res %d", result);
        return result;
    }

    auto image = std::make_unique<struct image>();
    image->handle = *pImage;
    image->format = pCreateInfo->format;
    image->device = dev;
    image->create_info = *pCreateInfo;
    image->emulate_sparse_binding = emulate_sparse_binding;

    if (emulate_sparse_binding) {
        result = BindSparseImage(dev, &create_info, pAllocator, image.get());
        if (result != VK_SUCCESS) {
            Logger::log("error", "Failed to bind sparse image, res %d", result);
            *pImage = VK_NULL_HANDLE;
            return result;
        }
    }

    {
        scoped_lock l(global_lock);
        imagesMap[*pImage] = std::move(image);
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateImageView(
    VkDevice device, const VkImageViewCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkImageView *pImageView) {
    VkResult result;
    struct device *dev = get_device(device);
    result =
        dev->table.CreateImageView(device, pCreateInfo, pAllocator, pImageView);
    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to create image view, res %d", result);
        return result;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyImage(
    VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator) {
    scoped_lock l(global_lock);

    struct device *dev = get_device(device);
    struct image *img = find_image(image);
    if (!dev || !img)
        return;
    dev->table.DestroyImage(device, image, pAllocator);
    if (img->emulate_sparse_binding) {
        DestroySparseImage(dev, image, pAllocator);
    }
    imagesMap.erase(image);
}
