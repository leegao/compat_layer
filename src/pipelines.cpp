#include "layer.hpp"
#include "logger.hpp"
#include "pipelines.hpp"
#include <vulkan/vulkan.h>

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreatePipelineLayout(
    VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkPipelineLayout *pPipelineLayout) {
    struct device *dev = get_device(device);

    VkResult result = dev->table.CreatePipelineLayout(
        device, pCreateInfo, pAllocator, pPipelineLayout);
    if (result != VK_SUCCESS) {
        Logger::log("error", "vkCreatePipelineLayout failed: %d", result);
        return result;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyPipelineLayout(
    VkDevice device, VkPipelineLayout pipelineLayout,
    const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);

    dev->table.DestroyPipelineLayout(device, pipelineLayout, pAllocator);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateGraphicsPipelines(
    VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
    const VkGraphicsPipelineCreateInfo *pCreateInfos,
    const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines) {
    struct device *dev = get_device(device);

    std::vector<VkGraphicsPipelineCreateInfo> createInfos(
        pCreateInfos, pCreateInfos + createInfoCount);
    VkResult result = dev->table.CreateGraphicsPipelines(
        device, pipelineCache, createInfoCount, createInfos.data(), pAllocator,
        pPipelines);

    if (result != VK_SUCCESS && result != VK_PIPELINE_COMPILE_REQUIRED) {
        Logger::log("error", "vkCreateGraphicsPipelines failed, result: %d",
                    result);
        return result;
    }

    return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateComputePipelines(
    VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
    const VkComputePipelineCreateInfo *pCreateInfos,
    const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines) {
    struct device *dev = get_device(device);

    std::vector<VkComputePipelineCreateInfo> createInfos(
        pCreateInfos, pCreateInfos + createInfoCount);
    VkResult result = dev->table.CreateComputePipelines(
        device, pipelineCache, createInfoCount, createInfos.data(), pAllocator,
        pPipelines);

    if (result != VK_SUCCESS && result != VK_PIPELINE_COMPILE_REQUIRED) {
        Logger::log("error", "vkCreateComputePipelines failed, result: %d",
                    result);
        return result;
    }

    return result;
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_DestroyPipeline(VkDevice device, VkPipeline pipeline,
                                    const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);

    dev->table.DestroyPipeline(device, pipeline, pAllocator);
}
