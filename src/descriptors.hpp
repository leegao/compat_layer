#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

struct BindingHint {
    VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

struct descriptor_set_layout {
    VkDescriptorSetLayout handle;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bool isEmulatedPushDescriptor = false;

    std::shared_mutex hintsLock;
    std::unordered_map<uint32_t, BindingHint> bindingHints;
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

using DescriptorSetsMap =
    std::unordered_map<VkDescriptorSet, VkDescriptorSetLayout>;
using DescriptorPoolsMap =
    std::unordered_map<VkDescriptorPool, std::vector<VkDescriptorSet>>;

extern std::shared_mutex descriptorSetsLock;
extern DescriptorSetsMap descriptorSetsMap;

extern std::shared_mutex descriptorPoolsLock;
extern DescriptorPoolsMap descriptorPoolsMap;

void track_descriptor_sets(VkDescriptorPool pool, uint32_t count,
                           const VkDescriptorSetLayout *pLayouts,
                           const VkDescriptorSet *pSets);
void untrack_descriptor_sets(uint32_t count, const VkDescriptorSet *pSets);
void untrack_descriptor_pool(VkDescriptorPool pool);
VkDescriptorSetLayout get_layout_for_set(VkDescriptorSet set);
