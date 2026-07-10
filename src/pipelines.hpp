#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct pipeline_layout {
    VkPipelineLayout handle;
    std::vector<VkDescriptorSetLayout> setLayouts;
};

extern std::unordered_map<VkPipelineLayout, std::unique_ptr<pipeline_layout>>
    pipelineLayoutsMap;
pipeline_layout *get_pipeline_layout(VkPipelineLayout layout);
