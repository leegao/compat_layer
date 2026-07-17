#pragma once

#include "layer.hpp"
#include <functional>
#include <string_view>
#include <vulkan/vulkan.h>

struct queue {
    VkQueue handle;
    struct device *device;
    uint32_t queueFamilyIndex;
    uint32_t queueIndex;
};

struct queue *get_queue(VkQueue queue);

VkResult SubmitOneShot(struct device *dev,
                       std::function<void(struct command_buffer *)> record_func,
                       const std::string_view shader_label,
                       struct queue *q = nullptr,
                       const std::vector<VkSemaphore> &waits = {},
                       const std::vector<VkSemaphore> &signals = {},
                       VkFence signal_fence = VK_NULL_HANDLE,
                       bool async = false);

VkResult
SubmitOneShotAsync(struct device *dev,
                   std::function<void(struct command_buffer *)> record_func,
                   const std::string_view shader_label,
                   struct queue *q = nullptr,
                   const std::vector<VkSemaphore> &waits = {},
                   const std::vector<VkSemaphore> &signals = {},
                   VkFence signal_fence = VK_NULL_HANDLE);
