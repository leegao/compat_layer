#include "descriptors.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "null_descriptors.hpp"
#include <mutex>
#include <shared_mutex>

std::shared_mutex descriptorSetLayoutsLock;

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

    if (pCreateInfo->pBindings && pCreateInfo->bindingCount > 0) {
        for (uint32_t i = 0; i < pCreateInfo->bindingCount; ++i) {
            VkDescriptorType type = pCreateInfo->pBindings[i].descriptorType;
            if (pCreateInfo->pBindings[i].descriptorCount >= 100000) {
                Logger::log(
                    "info",
                    "Binding %u exceeds 100000 descriptors: descriptorType=%u, "
                    "descriptorCount=%u",
                    i, type, pCreateInfo->pBindings[i].descriptorCount);
                ((VkDescriptorSetLayoutBinding *)(createInfo.pBindings))[i]
                    .descriptorCount = 100000;
            }
        }
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
        std::unique_lock l(descriptorSetLayoutsLock);
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
        std::unique_lock l(descriptorSetLayoutsLock);
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

    if (pCreateInfo->pDescriptorUpdateEntries &&
        pCreateInfo->descriptorUpdateEntryCount > 0) {
        descriptor_template->entries.reserve(
            pCreateInfo->descriptorUpdateEntryCount);
        for (uint32_t i = 0; i < pCreateInfo->descriptorUpdateEntryCount; ++i) {
            const auto src = pCreateInfo->pDescriptorUpdateEntries[i];
            descriptor_template->entries.push_back(src);
        }
    }

    {
        std::unique_lock l(descriptorSetLayoutsLock);
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
        std::unique_lock l(descriptorSetLayoutsLock);
        descriptorUpdateTemplatesMap.erase(descriptorUpdateTemplate);
    }
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_UpdateDescriptorSets(
    VkDevice device, uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount,
    const VkCopyDescriptorSet *pDescriptorCopies) {
    struct device *dev = get_device(device);

    if (!dev->emulate_null_descriptor || descriptorWriteCount == 0) {
        dev->table.UpdateDescriptorSets(device, descriptorWriteCount,
                                        pDescriptorWrites, descriptorCopyCount,
                                        pDescriptorCopies);
        return;
    }

    fix_null_descriptors(
        dev, descriptorWriteCount, pDescriptorWrites,
        [&](const VkWriteDescriptorSet *patchedDescriptorWrites) {
            dev->table.UpdateDescriptorSets(
                device, descriptorWriteCount, patchedDescriptorWrites,
                descriptorCopyCount, pDescriptorCopies);
        });
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_UpdateDescriptorSetWithTemplate(
    VkDevice device, VkDescriptorSet descriptorSet,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData) {
    struct device *dev = get_device(device);

    auto *updateTemplate =
        get_descriptor_update_template(descriptorUpdateTemplate);

    if (!dev->emulate_null_descriptor || !updateTemplate ||
        updateTemplate->entries.empty() || !pData) {
        dev->table.UpdateDescriptorSetWithTemplate(
            device, descriptorSet, descriptorUpdateTemplate, pData);
        return;
    }

    fix_null_descriptor_templates(
        dev, updateTemplate, pData, [&](const void *data) {
            dev->table.UpdateDescriptorSetWithTemplate(
                device, descriptorSet, descriptorUpdateTemplate, data);
        });
}
