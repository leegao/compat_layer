#include "command_buffer.hpp"
#include "descriptors.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "pipelines.hpp"

DescriptorSetLayoutsMap descriptorSetLayoutsMap;
DescriptorUpdateTemplatesMap descriptorUpdateTemplatesMap;

descriptor_set_layout *get_descriptor_set_layout(VkDescriptorSetLayout layout) {
    auto it = descriptorSetLayoutsMap.find(layout);
    if (it == descriptorSetLayoutsMap.end())
        return nullptr;
    return it->second.get();
}

descriptor_update_template *
get_descriptor_update_template(VkDescriptorUpdateTemplate temp) {
    auto it = descriptorUpdateTemplatesMap.find(temp);
    if (it == descriptorUpdateTemplatesMap.end())
        return nullptr;
    return it->second.get();
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DxvkMaliCompatLayer_CreateDescriptorSetLayout(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDescriptorSetLayout *pSetLayout) {
    struct device *dev = get_device(device);

    VkDescriptorSetLayoutCreateInfo createInfo = *pCreateInfo;
    bool isPushDescriptor = false;
    if (createInfo.flags &
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) {

        Logger::log("info",
                    "vkCreateDescriptorSetLayout: "
                    "VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR "
                    "device=%p createInfo=%p",
                    device, pCreateInfo);
        isPushDescriptor = true;
        createInfo.flags &=
            ~VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    }

    VkResult result = dev->table.CreateDescriptorSetLayout(
        device, &createInfo, pAllocator, pSetLayout);
    if (result != VK_SUCCESS) {
        Logger::log("error", "vkCreateDescriptorSetLayout failed, result: %d",
                    result);
        return result;
    }

    auto layout = std::make_unique<descriptor_set_layout>();
    layout->handle = *pSetLayout;
    layout->isPushDescriptor = isPushDescriptor;
    if (pCreateInfo->pBindings && pCreateInfo->bindingCount > 0) {
        layout->bindings.assign(pCreateInfo->pBindings,
                                pCreateInfo->pBindings +
                                    pCreateInfo->bindingCount);
    }

    {
        scoped_lock l(global_lock);
        descriptorSetLayoutsMap[*pSetLayout] = std::move(layout);
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyDescriptorSetLayout(
    VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
    const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);

    dev->table.DestroyDescriptorSetLayout(device, descriptorSetLayout,
                                          pAllocator);

    {
        scoped_lock l(global_lock);
        descriptorSetLayoutsMap.erase(descriptorSetLayout);
    }
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DxvkMaliCompatLayer_CreateDescriptorUpdateTemplate(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate) {
    struct device *dev = get_device(device);

    VkDescriptorUpdateTemplateCreateInfo createInfo = *pCreateInfo;
    bool isPushDescriptor = false;
    if (createInfo.templateType ==
        VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR) {
        isPushDescriptor = true;
        createInfo.templateType =
            VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
    }

    VkResult result = dev->table.CreateDescriptorUpdateTemplate(
        device, &createInfo, pAllocator, pDescriptorUpdateTemplate);
    if (result != VK_SUCCESS) {
        Logger::log("error",
                    "vkCreateDescriptorUpdateTemplate failed, result: %d",
                    result);
        return result;
    }

    auto descriptor_template = std::make_unique<descriptor_update_template>();
    descriptor_template->handle = *pDescriptorUpdateTemplate;
    descriptor_template->layout = pCreateInfo->descriptorSetLayout;
    descriptor_template->pipelineBindPoint = pCreateInfo->pipelineBindPoint;
    descriptor_template->isPushDescriptor = isPushDescriptor;

    {
        scoped_lock l(global_lock);
        descriptorUpdateTemplatesMap[*pDescriptorUpdateTemplate] =
            std::move(descriptor_template);
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_DestroyDescriptorUpdateTemplate(
    VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);

    dev->table.DestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate,
                                               pAllocator);

    {
        scoped_lock l(global_lock);
        descriptorUpdateTemplatesMap.erase(descriptorUpdateTemplate);
    }
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_CmdPushDescriptorSetKHR(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet *pDescriptorWrites) {
    auto *cb = get_command_buffer(commandBuffer);
    auto *dev = cb->device;
    auto *pipeline_layout = get_pipeline_layout(layout);
    if (!pipeline_layout || set >= pipeline_layout->setLayouts.size()) {
        Logger::log(
            "error",
            "vkCmdPushDescriptorSetKHR: pipeline layout or set index invalid");
        return;
    }

    auto layoutHandle = pipeline_layout->setLayouts[set];
    auto *descriptor_set_layout = get_descriptor_set_layout(layoutHandle);

    if (!descriptor_set_layout || !descriptor_set_layout->isPushDescriptor) {
        Logger::log("error", "vkCmdPushDescriptorSetKHR: descriptor set layout "
                             "not found or is not a push descriptor set");
        return;
    }

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet allocatedSet = VK_NULL_HANDLE;
    VkResult res = dev->descriptorSetAllocator->allocate(layoutHandle, &pool,
                                                         &allocatedSet);
    if (res != VK_SUCCESS) {
        Logger::log("error",
                    "vkCmdPushDescriptorSetKHR: "
                    "DescriptorSetAllocator::allocate failed: %d",
                    res);
        return;
    }

    // TODO: work out the proper lifecycle
    // cb->currentStagingResources->AddDescriptorSet(pool, allocatedSet);

    std::vector<VkWriteDescriptorSet> updates(
        pDescriptorWrites, pDescriptorWrites + descriptorWriteCount);
    for (uint32_t i = 0; i < descriptorWriteCount; ++i) {
        updates[i].dstSet = allocatedSet;
    }

    dev->table.UpdateDescriptorSets(dev->handle, descriptorWriteCount,
                                    updates.data(), 0, nullptr);
    dev->table.CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout,
                                     set, 1, &allocatedSet, 0, nullptr);

    // TODO: track graphics bind points too
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
    if (!cb)
        return;

    struct device *dev = cb->device;

    auto *descriptor_update_template =
        get_descriptor_update_template(descriptorUpdateTemplate);

    if (!descriptor_update_template) {
        Logger::log("error", "vkCmdPushDescriptorSetWithTemplateKHR: template "
                             "wrapper not found");
        return;
    }

    auto *descriptor_set_layout =
        get_descriptor_set_layout(descriptor_update_template->layout);
    if (!descriptor_set_layout || !descriptor_set_layout->isPushDescriptor) {
        Logger::log("error", "vkCmdPushDescriptorSetWithTemplateKHR: layout "
                             "not found or is not a push descriptor set");
        return;
    }

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet allocatedSet = VK_NULL_HANDLE;
    VkResult res = dev->descriptorSetAllocator->allocate(
        descriptor_set_layout->handle, &pool, &allocatedSet);
    if (res != VK_SUCCESS) {
        Logger::log("error",
                    "vkCmdPushDescriptorSetWithTemplateKHR: "
                    "DescriptorSetAllocator::allocate failed: %d",
                    res);
        return;
    }

    // TODO: work out the proper lifecycle
    // cb->currentStagingResources->AddDescriptorSet(pool, allocatedSet);
    dev->table.UpdateDescriptorSetWithTemplate(dev->handle, allocatedSet,
                                               descriptorUpdateTemplate, pData);
    VkPipelineBindPoint pipelineBindPoint =
        descriptor_update_template->pipelineBindPoint;
    dev->table.CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout,
                                     set, 1, &allocatedSet, 0, nullptr);

    if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
        cb->computePipelineState.TrackDescriptorSets(layout, set, 1,
                                                     &allocatedSet, 0, nullptr);
    }
}
