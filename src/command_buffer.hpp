#pragma once

#include "buffer.hpp"
#include "pipeline_state.hpp"
#include "staging_resources.hpp"
#include <memory>

struct command_buffer {
    VkCommandBuffer handle;
    struct device *device;
    VkCommandPool pool;
    struct fence *fence;
    std::unique_ptr<StagingResources> currentStagingResources;
    ComputePipelineBindingsState computePipelineState;
    std::vector<std::pair<VkDescriptorPool, VkDescriptorSet>>
        liveDescriptorSets;

    void reset_compute_state() { computePipelineState.reset(); }
    void kill_descriptor_sets();
};

struct command_buffer *get_command_buffer(VkCommandBuffer);
