#pragma once
#include "command_buffer.hpp"
#include "descriptors.hpp"
#include <vulkan/vulkan.h>

const descriptor_set_layout *GetDescriptorSetLayout(VkPipelineLayout layout,
                                                    uint32_t set);

const descriptor_set_layout *
GetDescriptorSetLayout(VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                       VkPipelineBindPoint &pipelineBindPoint);

VkDescriptorSet VKAPI_CALL BindEmulatedPushDescriptorSet(
    command_buffer *cb, VkPipelineBindPoint pipelineBindPoint,
    VkDescriptorSetLayout descriptorSetLayout, VkPipelineLayout layout,
    uint32_t set, uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet *pDescriptorWrites);

VkDescriptorSet VKAPI_CALL BindEmulatedPushDescriptorSetTemplate(
    command_buffer *cb, VkPipelineBindPoint pipelineBindPoint,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    VkDescriptorSetLayout descriptorSetLayout, VkPipelineLayout layout,
    uint32_t set, const void *pData);
