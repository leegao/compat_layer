#include "command_buffer.hpp"

#include "descriptors.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "pipeline_state.hpp"
#include "pipelines.hpp"
#include "push_descriptors.hpp"
#include "staging_resources.hpp"
#include "vk_func.hpp"
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

    auto *descriptorSetLayout = GetDescriptorSetLayout(layout, set);
    if (dev->emulate_push_descriptors && !descriptorSetLayout) {
        Logger::log(
            "error",
            "vkCmdPushDescriptorSetKHR: descriptor set layout not found");
        if (dev->table.CmdPushDescriptorSet) {
            dev->table.CmdPushDescriptorSet(commandBuffer, pipelineBindPoint,
                                            layout, set, descriptorWriteCount,
                                            pDescriptorWrites);
        }
        return;
    }

    if (!descriptorSetLayout->isEmulatedPushDescriptor) {
        dev->table.CmdPushDescriptorSet(commandBuffer, pipelineBindPoint,
                                        layout, set, descriptorWriteCount,
                                        pDescriptorWrites);
    }

    // Emulated path
    auto allocatedSet = BindEmulatedPushDescriptorSet(
        cb, pipelineBindPoint, descriptorSetLayout->handle, layout, set,
        descriptorWriteCount, pDescriptorWrites);

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

    VkPipelineBindPoint pipelineBindPoint;
    auto *descriptorSetLayout =
        GetDescriptorSetLayout(descriptorUpdateTemplate, pipelineBindPoint);
    if (dev->emulate_push_descriptors && !descriptorSetLayout) {
        Logger::log("error", "CmdPushDescriptorSetWithTemplate: descriptor set "
                             "layout not found");
        if (dev->table.CmdPushDescriptorSetWithTemplate) {
            dev->table.CmdPushDescriptorSetWithTemplate(
                commandBuffer, descriptorUpdateTemplate, layout, set, pData);
        }
        return;
    }

    if (!descriptorSetLayout->isEmulatedPushDescriptor) {
        dev->table.CmdPushDescriptorSetWithTemplate(
            commandBuffer, descriptorUpdateTemplate, layout, set, pData);
    }

    // Emulated path
    auto allocatedSet = BindEmulatedPushDescriptorSetTemplate(
        cb, pipelineBindPoint, descriptorUpdateTemplate,
        descriptorSetLayout->handle, layout, set, pData);

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
