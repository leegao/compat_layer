#include "descriptors.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "null_descriptors.hpp"
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <vulkan/vulkan.h>

std::shared_mutex descriptorSetLayoutsLock;

DescriptorSetLayoutsMap descriptorSetLayoutsMap;
DescriptorUpdateTemplatesMap descriptorUpdateTemplatesMap;

std::shared_mutex descriptorSetsLock;
DescriptorSetsMap descriptorSetsMap;

std::shared_mutex descriptorPoolsLock;
DescriptorPoolsMap descriptorPoolsMap;

void track_descriptor_sets(VkDescriptorPool pool, uint32_t count,
                           const VkDescriptorSetLayout *pLayouts,
                           const VkDescriptorSet *pSets) {
    {
        std::unique_lock l(descriptorSetsLock); // writer
        for (uint32_t i = 0; i < count; ++i) {
            descriptorSetsMap[pSets[i]] = pLayouts[i];
        }
    }

    {
        std::unique_lock l(descriptorPoolsLock); // writer
        auto &pool_sets = descriptorPoolsMap[pool];
        pool_sets.insert(pool_sets.end(), pSets, pSets + count);
    }
}

void untrack_descriptor_sets(uint32_t count, const VkDescriptorSet *pSets) {
    {
        std::unique_lock l(descriptorSetsLock); // writer
        for (uint32_t i = 0; i < count; ++i) {
            descriptorSetsMap.erase(pSets[i]);
        }
    }

    {
        std::unique_lock l_pool(descriptorPoolsLock); // writer
        for (auto &pair : descriptorPoolsMap) {
            auto &vec = pair.second;
            for (uint32_t i = 0; i < count; ++i) {
                vec.erase(std::remove(vec.begin(), vec.end(), pSets[i]),
                          vec.end());
            }
        }
    }
}

void untrack_descriptor_pool(VkDescriptorPool pool) {
    std::vector<VkDescriptorSet> sets_to_remove;
    {
        std::unique_lock l_pool(descriptorPoolsLock);
        auto it = descriptorPoolsMap.find(pool);
        if (it != descriptorPoolsMap.end()) {
            sets_to_remove = std::move(it->second);
            descriptorPoolsMap.erase(it);
        }
    }
    if (!sets_to_remove.empty()) {
        std::unique_lock l(descriptorSetsLock);
        for (auto set : sets_to_remove) {
            descriptorSetsMap.erase(set);
        }
    }
}

VkDescriptorSetLayout get_layout_for_set(VkDescriptorSet set) {
    auto it = descriptorSetsMap.find(set);
    if (it != descriptorSetsMap.end()) {
        return it->second;
    }
    return VK_NULL_HANDLE;
}

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

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_AllocateDescriptorSets(
    VkDevice device, const VkDescriptorSetAllocateInfo *pAllocateInfo,
    VkDescriptorSet *pDescriptorSets) {
    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_UNKNOWN;

    VkResult result = dev->table.AllocateDescriptorSets(device, pAllocateInfo,
                                                        pDescriptorSets);
    if (result == VK_SUCCESS) {
        track_descriptor_sets(pAllocateInfo->descriptorPool,
                              pAllocateInfo->descriptorSetCount,
                              pAllocateInfo->pSetLayouts, pDescriptorSets);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_FreeDescriptorSets(
    VkDevice device, VkDescriptorPool descriptorPool,
    uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets) {
    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_UNKNOWN;

    VkResult result = dev->table.FreeDescriptorSets(
        device, descriptorPool, descriptorSetCount, pDescriptorSets);
    if (result == VK_SUCCESS) {
        untrack_descriptor_sets(descriptorSetCount, pDescriptorSets);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_ResetDescriptorPool(
    VkDevice device, VkDescriptorPool descriptorPool,
    VkDescriptorPoolResetFlags flags) {
    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_UNKNOWN;

    VkResult result =
        dev->table.ResetDescriptorPool(device, descriptorPool, flags);
    if (result == VK_SUCCESS) {
        untrack_descriptor_pool(descriptorPool);
    }
    return result;
}

VK_LAYER_EXPORT void VKAPI_CALL DxvkMaliCompatLayer_DestroyDescriptorPool(
    VkDevice device, VkDescriptorPool descriptorPool,
    const VkAllocationCallbacks *pAllocator) {
    struct device *dev = get_device(device);
    if (!dev)
        return;

    untrack_descriptor_pool(descriptorPool);
    dev->table.DestroyDescriptorPool(device, descriptorPool, pAllocator);
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
