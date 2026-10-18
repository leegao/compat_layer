#include "layer.hpp"
#include "logger.hpp"
#include "samplers.hpp"
#include <mutex>
#include <unordered_set>
#include <vulkan/vulkan.h>

std::shared_mutex ycbcrSamplersLock;
std::unordered_set<VkSampler> ycbcrSamplers;

bool IsYcbcrSampler(VkSampler sampler) {
    std::shared_lock l(ycbcrSamplersLock);
    return ycbcrSamplers.find(sampler) != ycbcrSamplers.end();
}

VkSampler GetYcbcrFallbackSampler(struct device *dev,
                                  const VkSamplerCreateInfo *pCreateInfo) {
    {
        std::shared_lock l(dev->fallbackSamplerLock);
        if (dev->fallbackSampler != VK_NULL_HANDLE) {
            return dev->fallbackSampler;
        }
    }

    VkSamplerCreateInfo createInfo = *pCreateInfo;
    createInfo.pNext = nullptr;

    VkSampler fallback = VK_NULL_HANDLE;
    VkResult result =
        dev->table.CreateSampler(dev->handle, &createInfo, nullptr, &fallback);
    if (result == VK_SUCCESS) {
        std::unique_lock l(dev->fallbackSamplerLock); // writer
        dev->fallbackSampler = fallback;
    } else {
        Logger::log(
            "error",
            "Failed to create YCbCr fallback linear sampler, result = %d",
            result);
    }
    return fallback;
}

void DestroyYcbcrFallbackSampler(struct device *dev) {
    if (dev->fallbackSampler != VK_NULL_HANDLE) {
        std::unique_lock l(dev->fallbackSamplerLock); // writer
        dev->table.DestroySampler(dev->handle, dev->fallbackSampler, nullptr);
        dev->fallbackSampler = VK_NULL_HANDLE;
    }
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateSampler(
    VkDevice device, const VkSamplerCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkSampler *pSampler) {
    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_UNKNOWN;

    VkResult result =
        dev->table.CreateSampler(device, pCreateInfo, pAllocator, pSampler);
    if (result != VK_SUCCESS) {
        Logger::log("error", "vkCreateSampler failed, result = %d", result);
        return result;
    }

    bool isYcbcr = false;
    for (const auto *header =
             static_cast<const VkBaseInStructure *>(pCreateInfo->pNext);
         header != nullptr; header = header->pNext) {
        if (header->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO) {
            isYcbcr = true;
            break;
        }
    }

    if (isYcbcr) {
        {
            std::unique_lock l(ycbcrSamplersLock); // writer
            ycbcrSamplers.insert(*pSampler);
        }
        // prewarm (with only this pCreateInfo)
        GetYcbcrFallbackSampler(dev, pCreateInfo);
        Logger::log("info", "YCbCr sampler not supported for compute, falling "
                            "back to a linear sampler");
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_DestroySampler(VkDevice device, VkSampler sampler,
                                   const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);
    if (!dev)
        return;

    {
        std::unique_lock l(ycbcrSamplersLock); // writer
        ycbcrSamplers.erase(sampler);
    }

    dev->table.DestroySampler(device, sampler, pAllocator);
}
