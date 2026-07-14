#pragma once

#include "descriptors.hpp"
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct pipeline_layout {
    VkPipelineLayout handle;
    std::vector<VkDescriptorSetLayout> setLayouts;

    descriptor_set_layout *GetDescriptorSetLayout(uint32_t set) {
        if (set >= setLayouts.size()) {
            return nullptr;
        }
        return get_descriptor_set_layout(setLayouts[set]);
    }
};

extern std::shared_mutex pipelineLayoutsLock;
extern std::unordered_map<VkPipelineLayout, std::unique_ptr<pipeline_layout>>
    pipelineLayoutsMap;
pipeline_layout *get_pipeline_layout(VkPipelineLayout layout);
