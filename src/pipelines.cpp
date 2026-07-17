#include "layer.hpp"
#include "logger.hpp"
#include "pipelines.hpp"
#include "shader_reflector.hpp"
#include <vulkan/vulkan.h>

std::shared_mutex pipelineLayoutsLock;
std::unordered_map<VkPipelineLayout, std::unique_ptr<pipeline_layout>>
    pipelineLayoutsMap;

pipeline_layout *get_pipeline_layout(VkPipelineLayout layout) {
    auto it = pipelineLayoutsMap.find(layout);
    if (it != pipelineLayoutsMap.end()) {
        return it->second.get();
    }
    return nullptr;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateShaderModule(
    VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule) {
    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_UNKNOWN;

    VkResult result = dev->table.CreateShaderModule(device, pCreateInfo,
                                                    pAllocator, pShaderModule);
    if (result != VK_SUCCESS) {
        Logger::log("error", "vkCreateShaderModule failed: %d", result);
        return result;
    }

    if (dev->emulate_precise_null_descriptor)
        TrackShader(dev, *pShaderModule, pCreateInfo);
    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyShaderModule(
    VkDevice device, VkShaderModule shaderModule,
    const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);
    if (!dev)
        return;

    if (dev->emulate_precise_null_descriptor)
        UntrackShader(shaderModule);
    dev->table.DestroyShaderModule(device, shaderModule, pAllocator);
}

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

    auto pipelineLayout = std::make_unique<pipeline_layout>();
    pipelineLayout->handle = *pPipelineLayout;
    if (pCreateInfo->pSetLayouts && pCreateInfo->setLayoutCount > 0) {
        pipelineLayout->setLayouts.assign(pCreateInfo->pSetLayouts,
                                          pCreateInfo->pSetLayouts +
                                              pCreateInfo->setLayoutCount);
    }

    {
        std::unique_lock l(pipelineLayoutsLock);
        pipelineLayoutsMap[*pPipelineLayout] = std::move(pipelineLayout);
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyPipelineLayout(
    VkDevice device, VkPipelineLayout pipelineLayout,
    const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);

    dev->table.DestroyPipelineLayout(device, pipelineLayout, pAllocator);

    {
        std::unique_lock l(pipelineLayoutsLock);
        pipelineLayoutsMap.erase(pipelineLayout);
    }
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

    if (result == VK_SUCCESS && dev->emulate_precise_null_descriptor) {
        for (int i = 0; i < createInfoCount; i++) {
            TrackPipelineDescriptorLayoutBindingTypes(
                dev, pCreateInfos[i].layout, pCreateInfos[i].stageCount,
                pCreateInfos[i].pStages);
        }
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

    if (result == VK_SUCCESS && dev->emulate_precise_null_descriptor) {
        for (uint32_t i = 0; i < createInfoCount; ++i) {
            TrackPipelineDescriptorLayoutBindingTypes(
                dev, pCreateInfos[i].layout, 1, &pCreateInfos[i].stage);
        }
    }

    return result;
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_DestroyPipeline(VkDevice device, VkPipeline pipeline,
                                    const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);

    dev->table.DestroyPipeline(device, pipeline, pAllocator);
}
