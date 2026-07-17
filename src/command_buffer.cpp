#include "command_buffer.hpp"

#include "descriptors.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "pipeline_state.hpp"
#include "pipelines.hpp"
#include "staging_resources.hpp"
#include <algorithm>
#include <unordered_map>

using CommandBuffersMap =
    std::unordered_map<VkCommandBuffer, std::shared_ptr<struct command_buffer>>;
using CommandPoolsMap =
    std::unordered_map<VkCommandPool, std::vector<VkCommandBuffer>>;

CommandBuffersMap commandBuffersMap;
CommandPoolsMap commandPoolsMap;

struct command_buffer *get_command_buffer(VkCommandBuffer commandbuffer) {
    auto it = commandBuffersMap.find(commandbuffer);

    if (it == commandBuffersMap.end())
        return nullptr;

    return it->second.get();
}

void command_buffer::kill_descriptor_sets() {
    if (liveDescriptorSets.empty()) {
        return;
    }
    for (auto [pool, set] : liveDescriptorSets) {
        device->descriptorSetAllocator->free(pool, set);
    }
    liveDescriptorSets.clear();
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_AllocateCommandBuffers(
    VkDevice device, const VkCommandBufferAllocateInfo *pAllocateInfo,
    VkCommandBuffer *pCommandBuffers) {
    VkResult result;
    VkLayerDispatchTable table;

    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_INITIALIZATION_FAILED;

    table = dev->table;

    result =
        table.AllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
    if (result != VK_SUCCESS) {
        Logger::log("error", "Failed to allocate command buffers, res %d",
                    result);
        return result;
    }

    for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; i++) {
        auto cmd = std::make_shared<struct command_buffer>();
        cmd->handle = pCommandBuffers[i];
        cmd->device = dev;
        cmd->pool = pAllocateInfo->commandPool;
        cmd->currentStagingResources =
            std::make_unique<StagingResources>(device);
        cmd->reset_compute_state();
        {
            scoped_lock l(global_lock);
            commandBuffersMap[pCommandBuffers[i]] = cmd;
            commandPoolsMap[pAllocateInfo->commandPool].push_back(
                pCommandBuffers[i]);
        }

        // See
        // https://vulkan.lunarg.com/doc/view/latest/linux/LoaderLayerInterface.html#creating-new-dispatchable-objects
        if (dev->has_more_layers) {
            *reinterpret_cast<void **>(pCommandBuffers[i]) =
                *reinterpret_cast<void **>(device);
        }
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_FreeCommandBuffers(
    VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount,
    const VkCommandBuffer *pCommandBuffers) {
    scoped_lock l(global_lock);

    struct device *dev = get_device(device);
    if (!dev)
        return;

    auto &poolBuffers = commandPoolsMap[commandPool];
    for (uint32_t i = 0; i < commandBufferCount; i++) {
        struct command_buffer *cb = get_command_buffer(pCommandBuffers[i]);
        if (!cb)
            continue;

        cb->kill_descriptor_sets();

        dev->table.FreeCommandBuffers(dev->handle, commandPool, 1, &cb->handle);
        commandBuffersMap.erase(pCommandBuffers[i]);
        poolBuffers.erase(std::remove(poolBuffers.begin(), poolBuffers.end(),
                                      pCommandBuffers[i]),
                          poolBuffers.end());
    }
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_BeginCommandBuffer(
    VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *pBeginInfo) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    struct device *dev = cb->device;

    cb->reset_compute_state(); // begin/reset should clear inherited compute
                               // pipeline states

    {
        scoped_lock l(global_lock);
        cb->kill_descriptor_sets(); // clear any live descriptor sets
    }

    return dev->table.BeginCommandBuffer(commandBuffer, pBeginInfo);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_ResetCommandBuffer(
    VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    struct device *dev = cb->device;

    cb->reset_compute_state(); // begin/reset should clear inherited compute
                               // pipeline states

    {
        scoped_lock l(global_lock);
        cb->kill_descriptor_sets(); // clear any live descriptor sets
    }

    return dev->table.ResetCommandBuffer(commandBuffer, flags);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_ResetCommandPool(
    VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    struct device *dev = get_device(device);

    {
        scoped_lock l(global_lock);
        auto &commandBuffers = commandPoolsMap[commandPool];
        for (auto &commandBuffer : commandBuffers) {
            auto *cb = get_command_buffer(commandBuffer);
            if (cb) {
                cb->kill_descriptor_sets();
                cb->reset_compute_state();
            }
        }
    }

    return dev->table.ResetCommandPool(device, commandPool, flags);
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyCommandPool(
    VkDevice device, VkCommandPool commandPool,
    const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);
    if (!dev)
        return;

    {
        scoped_lock l(global_lock);
        auto &commandBuffers = commandPoolsMap[commandPool];
        for (auto &commandBuffer : commandBuffers) {
            auto *cb = get_command_buffer(commandBuffer);
            if (cb) {
                cb->kill_descriptor_sets();
            }
        }
        commandPoolsMap.erase(commandPool);
    }

    dev->table.DestroyCommandPool(device, commandPool, pAllocator);
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_CmdBindPipeline(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipeline pipeline) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    struct device *dev = cb->device;

    if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
        cb->computePipelineState.TrackPipeline(pipeline);
    }

    dev->table.CmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_CmdBindDescriptorSets(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
    const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount,
    const uint32_t *pDynamicOffsets) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    struct device *dev = cb->device;

    if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
        cb->computePipelineState.TrackDescriptorSets(
            layout, firstSet, descriptorSetCount, pDescriptorSets,
            dynamicOffsetCount, pDynamicOffsets);
    }

    dev->table.CmdBindDescriptorSets(
        commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount,
        pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_CmdBindDescriptorSets2(
    VkCommandBuffer commandBuffer,
    const VkBindDescriptorSetsInfo *pBindDescriptorSetsInfo) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    struct device *dev = cb->device;

    if (pBindDescriptorSetsInfo->stageFlags & VK_SHADER_STAGE_COMPUTE_BIT) {
        cb->computePipelineState.TrackDescriptorSets(
            pBindDescriptorSetsInfo->layout, pBindDescriptorSetsInfo->firstSet,
            pBindDescriptorSetsInfo->descriptorSetCount,
            pBindDescriptorSetsInfo->pDescriptorSets,
            pBindDescriptorSetsInfo->dynamicOffsetCount,
            pBindDescriptorSetsInfo->pDynamicOffsets);
    }

    dev->table.CmdBindDescriptorSets2(commandBuffer, pBindDescriptorSetsInfo);
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_CmdPushConstants(
    VkCommandBuffer commandBuffer, VkPipelineLayout layout,
    VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size,
    const void *pValues) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    struct device *dev = cb->device;

    cb->computePipelineState.TrackPushConstants(layout, stageFlags, offset,
                                                size, pValues);

    dev->table.CmdPushConstants(commandBuffer, layout, stageFlags, offset, size,
                                pValues);
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_CmdPushConstants2(
    VkCommandBuffer commandBuffer,
    const VkPushConstantsInfo *pPushConstantsInfo) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    struct device *dev = cb->device;

    cb->computePipelineState.TrackPushConstants(
        pPushConstantsInfo->layout, pPushConstantsInfo->stageFlags,
        pPushConstantsInfo->offset, pPushConstantsInfo->size,
        pPushConstantsInfo->pValues);

    dev->table.CmdPushConstants2(commandBuffer, pPushConstantsInfo);
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_CmdPushDescriptorSetKHR(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet *pDescriptorWrites) {
    auto *cb = get_command_buffer(commandBuffer);
    auto *dev = cb->device;

    auto *pipelineLayout = ({
        std::shared_lock l(pipelineLayoutsLock); // reader
        get_pipeline_layout(layout);
    });
    if (!pipelineLayout || set >= pipelineLayout->setLayouts.size()) {
        Logger::log(
            "error",
            "vkCmdPushDescriptorSetKHR: pipeline layout or set index invalid");
        if (dev->table.CmdPushDescriptorSet) {
            dev->table.CmdPushDescriptorSet(commandBuffer, pipelineBindPoint,
                                            layout, set, descriptorWriteCount,
                                            pDescriptorWrites);
        }
        return;
    }

    auto *descriptorSetLayout = ({
        std::shared_lock l(descriptorSetLayoutsLock); // reader
        pipelineLayout->GetDescriptorSetLayout(set);
    });
    if (!descriptorSetLayout ||
        !descriptorSetLayout->isEmulatedPushDescriptor) {
        if (!descriptorSetLayout)
            Logger::log("error",
                        "vkCmdPushDescriptorSetKHR: descriptor set layout "
                        "not found");
        if (dev->table.CmdPushDescriptorSet) {
            dev->table.CmdPushDescriptorSet(commandBuffer, pipelineBindPoint,
                                            layout, set, descriptorWriteCount,
                                            pDescriptorWrites);
        }
        return;
    }

    // Emulated path
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet allocatedSet = VK_NULL_HANDLE;
    VkResult res = dev->descriptorSetAllocator->allocate(
        descriptorSetLayout->handle, &pool, &allocatedSet);
    if (res != VK_SUCCESS) {
        Logger::log("error",
                    "vkCmdPushDescriptorSetKHR: "
                    "DescriptorSetAllocator::allocate failed: %d",
                    res);
        return;
    }

    // Because command buffers can be recycled, and we're performing
    // immediate descriptor set updates, we need to tie these to the
    // lifecycle of the command buffer and not the submission.
    // See https://github.com/leegao/compat_layer/issues/1
    {
        scoped_lock l(global_lock);
        cb->liveDescriptorSets.push_back({pool, allocatedSet});
    }

    std::vector<VkWriteDescriptorSet> updates(
        pDescriptorWrites, pDescriptorWrites + descriptorWriteCount);
    for (uint32_t i = 0; i < descriptorWriteCount; ++i) {
        updates[i].dstSet = allocatedSet;
    }

    // TODO(leegao): look into late-binding instead of immediate state changes
    dev->table.UpdateDescriptorSets(dev->handle, descriptorWriteCount,
                                    updates.data(), 0, nullptr);
    dev->table.CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout,
                                     set, 1, &allocatedSet, 0, nullptr);

    if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
        cb->computePipelineState.TrackDescriptorSets(layout, set, 1,
                                                     &allocatedSet, 0, nullptr);
    }
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_CmdPushDescriptorSetWithTemplateKHR(
    VkCommandBuffer commandBuffer,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    VkPipelineLayout layout, uint32_t set, const void *pData) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    struct device *dev = cb->device;

    auto *descriptor_update_template = ({
        std::shared_lock l(descriptorSetLayoutsLock); // reader
        get_descriptor_update_template(descriptorUpdateTemplate);
    });
    if (!descriptor_update_template) {
        Logger::log("error", "vkCmdPushDescriptorSetWithTemplateKHR: template "
                             "not found");
        if (dev->table.CmdPushDescriptorSetWithTemplate) {
            dev->table.CmdPushDescriptorSetWithTemplate(
                commandBuffer, descriptorUpdateTemplate, layout, set, pData);
        }
        return;
    }

    auto *descriptorSetLayout = ({
        std::shared_lock l(descriptorSetLayoutsLock); // reader
        get_descriptor_set_layout(descriptor_update_template->layout);
    });
    if (!descriptorSetLayout ||
        !descriptorSetLayout->isEmulatedPushDescriptor) {
        if (!descriptorSetLayout)
            Logger::log("error",
                        "vkCmdPushDescriptorSetWithTemplateKHR: layout "
                        "not found or is not a push descriptor set");
        if (dev->table.CmdPushDescriptorSetWithTemplate) {
            dev->table.CmdPushDescriptorSetWithTemplate(
                commandBuffer, descriptorUpdateTemplate, layout, set, pData);
        }
        return;
    }

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet allocatedSet = VK_NULL_HANDLE;
    VkResult res = dev->descriptorSetAllocator->allocate(
        descriptorSetLayout->handle, &pool, &allocatedSet);
    if (res != VK_SUCCESS) {
        Logger::log("error",
                    "vkCmdPushDescriptorSetWithTemplateKHR: "
                    "DescriptorSetAllocator::allocate failed: %d",
                    res);
        return;
    }

    // Because command buffers can be recycled, and we're performing
    // immediate descriptor set updates, we need to tie these to the
    // lifecycle of the command buffer and not the submission.
    // See https://github.com/leegao/compat_layer/issues/1
    {
        scoped_lock l(global_lock);
        cb->liveDescriptorSets.push_back({pool, allocatedSet});
    }

    auto pipelineBindPoint = descriptor_update_template->pipelineBindPoint;
    // TODO(leegao): look into late-binding instead of immediate state changes
    dev->table.UpdateDescriptorSetWithTemplate(dev->handle, allocatedSet,
                                               descriptorUpdateTemplate, pData);
    dev->table.CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout,
                                     set, 1, &allocatedSet, 0, nullptr);

    if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
        cb->computePipelineState.TrackDescriptorSets(layout, set, 1,
                                                     &allocatedSet, 0, nullptr);
    }
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_CmdBindVertexBuffers(
    VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount,
    const VkBuffer *pBuffers, const VkDeviceSize *pOffsets) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    if (!cb)
        return;
    struct device *dev = cb->device;

    if (dev->emulate_null_descriptor && pBuffers) {
        std::vector<VkBuffer> buffers(bindingCount);
        for (int i = 0; i < bindingCount; i++) {
            buffers[i] = pBuffers[i] != VK_NULL_HANDLE
                             ? pBuffers[i]
                             : dev->null_descriptors.null_buffer;
        }
        dev->table.CmdBindVertexBuffers(commandBuffer, firstBinding,
                                        bindingCount, buffers.data(), pOffsets);
    } else {
        dev->table.CmdBindVertexBuffers(commandBuffer, firstBinding,
                                        bindingCount, pBuffers, pOffsets);
    }
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_CmdBindVertexBuffers2(
    VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount,
    const VkBuffer *pBuffers, const VkDeviceSize *pOffsets,
    const VkDeviceSize *pSizes, const VkDeviceSize *pStrides) {
    struct command_buffer *cb = get_command_buffer(commandBuffer);
    if (!cb)
        return;
    struct device *dev = cb->device;

    if (dev->emulate_null_descriptor && pBuffers) {
        std::vector<VkBuffer> buffers(bindingCount);
        std::vector<VkDeviceSize> sizes;
        if (pSizes)
            sizes.assign(pSizes, pSizes + bindingCount);

        for (int i = 0; i < bindingCount; i++) {
            if (pBuffers[i] == VK_NULL_HANDLE) {
                buffers[i] = dev->null_descriptors.null_buffer;
                if (pSizes) {
                    // Arbitrary clamp to 65536
                    sizes[i] =
                        std::min(pSizes[i], static_cast<VkDeviceSize>(65536));
                }
            } else {
                buffers[i] = pBuffers[i];
            }
        }
        dev->table.CmdBindVertexBuffers2(
            commandBuffer, firstBinding, bindingCount, buffers.data(), pOffsets,
            pSizes ? sizes.data() : nullptr, pStrides);
    } else {
        dev->table.CmdBindVertexBuffers2(commandBuffer, firstBinding,
                                         bindingCount, pBuffers, pOffsets,
                                         pSizes, pStrides);
    }
}
