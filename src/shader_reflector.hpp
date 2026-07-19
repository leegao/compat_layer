#pragma once

#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct device;

struct DescriptorBindingInfo {
    uint32_t set;
    uint32_t binding;
    VkImageViewType imageViewType;
    VkFormat format;
};

struct ShaderModuleInfo {
    std::vector<DescriptorBindingInfo> bindings;
};

extern std::shared_mutex shaderModulesLock;
extern std::unordered_map<VkShaderModule, ShaderModuleInfo> shaderModulesMap;

void TrackShader(struct device *dev, VkShaderModule shaderModule,
                 const VkShaderModuleCreateInfo *pCreateInfo);
void UntrackShader(VkShaderModule shaderModule);

void TrackPipelineDescriptorLayoutBindingTypes(
    struct device *dev, VkPipelineLayout layoutHandle, uint32_t stageCount,
    const VkPipelineShaderStageCreateInfo *pStages);
