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
    bool isEmulatedPushDescriptor = false;
    if (dev->emulate_push_descriptors &&
        createInfo.flags &
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) {
        isEmulatedPushDescriptor = true;
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
    layout->isEmulatedPushDescriptor = isEmulatedPushDescriptor;
    if (isEmulatedPushDescriptor && pCreateInfo->pBindings &&
        pCreateInfo->bindingCount > 0) {
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
    bool isEmulatedPushDescriptor = false;
    if (dev->emulate_push_descriptors &&
        createInfo.templateType ==
            VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR) {
        isEmulatedPushDescriptor = true;
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
    descriptor_template->isEmulatedPushDescriptor = isEmulatedPushDescriptor;

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
