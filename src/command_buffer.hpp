#ifndef __COMMAND_BUFFER_HPP
#define __COMMAND_BUFFER_HPP

#include "buffer.hpp"
#include "pipeline_state.hpp"
#include "staging_resources.hpp"
#include <functional>
#include <string_view>

struct command_buffer {
    VkCommandBuffer handle;
    struct device *device;
    VkCommandPool pool;
    struct fence *fence;
    std::unique_ptr<StagingResources> currentStagingResources;
    ComputePipelineBindingsState computePipelineState;

    void reset_compute_state() { computePipelineState.reset(); }
};

struct command_buffer *get_command_buffer(VkCommandBuffer);

VkResult DispatchOneShotAndSample(
    struct device *dev,
    std::function<void(struct command_buffer *)> record_func,
    const std::string_view name);

#endif
