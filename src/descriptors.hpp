#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct descriptor_set_layout {
    VkDescriptorSetLayout handle;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bool isEmulatedPushDescriptor = false;
};

struct descriptor_update_template {
    VkDescriptorUpdateTemplate handle;
    VkDescriptorSetLayout layout;
    VkPipelineBindPoint pipelineBindPoint;
    bool isEmulatedPushDescriptor = false;
    std::vector<VkDescriptorUpdateTemplateEntry> entries;
};

using DescriptorSetLayoutsMap =
    std::unordered_map<VkDescriptorSetLayout,
                       std::unique_ptr<descriptor_set_layout>>;
using DescriptorUpdateTemplatesMap =
    std::unordered_map<VkDescriptorUpdateTemplate,
                       std::unique_ptr<descriptor_update_template>>;

extern std::shared_mutex descriptorSetLayoutsLock;
extern DescriptorSetLayoutsMap descriptorSetLayoutsMap;
extern DescriptorUpdateTemplatesMap descriptorUpdateTemplatesMap;

descriptor_set_layout *get_descriptor_set_layout(VkDescriptorSetLayout layout);
descriptor_update_template *
get_descriptor_update_template(VkDescriptorUpdateTemplate templ);
