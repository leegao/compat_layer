#include "push_descriptors.hpp"

#include "command_buffer.hpp"
#include "descriptors.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "pipelines.hpp"
#include "vk_func.hpp"
#include "vulkan/vk_layer.h"
#include <cstdint>
#include <optional>
#include <vulkan/vulkan.h>

namespace {

VkResult AllocateDescriptorSet(command_buffer *cb,
                               VkDescriptorSetLayout descriptorSetLayout,
                               VkDescriptorSet &allocatedSet) {
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = cb->device->descriptorSetAllocator->allocate(
        descriptorSetLayout, &pool, &allocatedSet);
    if (result != VK_SUCCESS) {
        Logger::log("error",
                    "AllocateDescriptorSet: "
                    "DescriptorSetAllocator::allocate failed: %d",
                    result);
        return result;
    }

    // Because command buffers can be recycled, and we're performing
    // immediate descriptor set updates, we need to tie these to the
    // lifecycle of the command buffer and not the submission.
    // See https://github.com/leegao/compat_layer/issues/1
    {
        scoped_lock l(global_lock);
        cb->liveDescriptorSets.push_back({pool, allocatedSet});
    }

    track_descriptor_sets(pool, 1, &descriptorSetLayout, &allocatedSet);
    return VK_SUCCESS;
}

} // namespace

const descriptor_set_layout *GetDescriptorSetLayout(VkPipelineLayout layout,
                                                    uint32_t set) {
    auto *pipelineLayout = ({
        std::shared_lock l(pipelineLayoutsLock); // reader
        get_pipeline_layout(layout);
    });
    if (!pipelineLayout || set >= pipelineLayout->setLayouts.size()) {
        return nullptr;
    }

    auto *descriptorSetLayout = ({
        std::shared_lock l(descriptorSetLayoutsLock); // reader
        pipelineLayout->GetDescriptorSetLayout(set);
    });
    if (!descriptorSetLayout) {
        return nullptr;
    }
    return descriptorSetLayout;
}

const descriptor_set_layout *
GetDescriptorSetLayout(VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                       VkPipelineBindPoint &pipelineBindPoint) {
    auto *descriptor_update_template = ({
        std::shared_lock l(descriptorSetLayoutsLock); // reader
        get_descriptor_update_template(descriptorUpdateTemplate);
    });
    if (!descriptor_update_template) {
        return nullptr;
    }

    auto *descriptorSetLayout = ({
        std::shared_lock l(descriptorSetLayoutsLock); // reader
        get_descriptor_set_layout(descriptor_update_template->layout);
    });
    if (!descriptorSetLayout) {
        return nullptr;
    }
    pipelineBindPoint = descriptor_update_template->pipelineBindPoint;
    return descriptorSetLayout;
}

VkDescriptorSet BindEmulatedPushDescriptorSet(
    command_buffer *cb, VkPipelineBindPoint pipelineBindPoint,
    VkDescriptorSetLayout descriptorSetLayout, VkPipelineLayout layout,
    uint32_t set, uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet *pDescriptorWrites) {
    auto *dev = cb->device;

    VkDescriptorSet allocatedSet = VK_NULL_HANDLE;
    auto result = AllocateDescriptorSet(cb, descriptorSetLayout, allocatedSet);
    if (result != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    std::vector<VkWriteDescriptorSet> updates(
        pDescriptorWrites, pDescriptorWrites + descriptorWriteCount);
    for (auto &update : updates)
        update.dstSet = allocatedSet;

    // TODO(leegao): look into late-binding instead of immediate state changes
    DxvkMaliCompatLayer_UpdateDescriptorSets(dev->handle, descriptorWriteCount,
                                             updates.data(), 0, nullptr);
    dev->table.CmdBindDescriptorSets(cb->handle, pipelineBindPoint, layout, set,
                                     1, &allocatedSet, 0, nullptr);
    return allocatedSet;
}

VkDescriptorSet BindEmulatedPushDescriptorSetTemplate(
    command_buffer *cb, VkPipelineBindPoint pipelineBindPoint,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    VkDescriptorSetLayout descriptorSetLayout, VkPipelineLayout layout,
    uint32_t set, const void *pData) {
    struct device *dev = cb->device;

    VkDescriptorSet allocatedSet = VK_NULL_HANDLE;
    auto result = AllocateDescriptorSet(cb, descriptorSetLayout, allocatedSet);
    if (result != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    // TODO(leegao): look into late-binding instead of immediate state changes
    DxvkMaliCompatLayer_UpdateDescriptorSetWithTemplate(
        dev->handle, allocatedSet, descriptorUpdateTemplate, pData);
    dev->table.CmdBindDescriptorSets(cb->handle, pipelineBindPoint, layout, set,
                                     1, &allocatedSet, 0, nullptr);

    return allocatedSet;
}
