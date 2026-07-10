#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct descriptor_set_layout {
    VkDescriptorSetLayout handle;
    bool isPushDescriptor = false;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

struct descriptor_update_template {
    VkDescriptorUpdateTemplate handle;
    VkDescriptorSetLayout layout;
    VkPipelineBindPoint pipelineBindPoint;
    bool isPushDescriptor = false;
};

using DescriptorSetLayoutsMap =
    std::unordered_map<VkDescriptorSetLayout,
                       std::unique_ptr<descriptor_set_layout>>;
using DescriptorUpdateTemplatesMap =
    std::unordered_map<VkDescriptorUpdateTemplate,
                       std::unique_ptr<descriptor_update_template>>;

extern DescriptorSetLayoutsMap descriptorSetLayoutsMap;
extern DescriptorUpdateTemplatesMap descriptorUpdateTemplatesMap;

descriptor_set_layout *get_descriptor_set_layout(VkDescriptorSetLayout layout);
descriptor_update_template *
get_descriptor_update_template(VkDescriptorUpdateTemplate templ);
