#include "descriptors.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include <cstring>
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
            const auto &src = pCreateInfo->pDescriptorUpdateEntries[i];
            descriptor_template->entries.push_back(
                {src.descriptorType, src.dstBinding, src.dstArrayElement,
                 src.descriptorCount, src.offset, src.stride});
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
    if (!dev)
        return;

    // Logger::log("info",
    //             "DxvkMaliCompatLayer_UpdateDescriptorSets: "
    //             "descriptorWriteCount=%u, emulate_null_desc=%d",
    //             descriptorWriteCount, dev->emulate_null_descriptor);
    if (!dev->emulate_null_descriptor || descriptorWriteCount == 0) {
        dev->table.UpdateDescriptorSets(device, descriptorWriteCount,
                                        pDescriptorWrites, descriptorCopyCount,
                                        pDescriptorCopies);
        return;
    }

    std::vector<VkWriteDescriptorSet> writes(
        pDescriptorWrites, pDescriptorWrites + descriptorWriteCount);
    std::vector<std::vector<VkDescriptorBufferInfo>> tempBufferInfos(
        descriptorWriteCount);
    std::vector<std::vector<VkDescriptorImageInfo>> tempImageInfos(
        descriptorWriteCount);
    std::vector<std::vector<VkBufferView>> tempTexelViews(descriptorWriteCount);

    for (uint32_t i = 0; i < descriptorWriteCount; i++) {
        const VkWriteDescriptorSet &w = pDescriptorWrites[i];
        uint32_t n = w.descriptorCount;

        switch (w.descriptorType) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
            if (w.pBufferInfo) {
                tempBufferInfos[i].assign(w.pBufferInfo, w.pBufferInfo + n);
                bool modified = false;
                for (uint32_t j = 0; j < n; j++) {
                    if (tempBufferInfos[i][j].buffer == VK_NULL_HANDLE) {
                        tempBufferInfos[i][j].buffer =
                            dev->null_descriptors.null_buffer;
                        tempBufferInfos[i][j].offset = 0;
                        tempBufferInfos[i][j].range = VK_WHOLE_SIZE;
                        modified = true;
                    }
                }
                if (modified)
                    writes[i].pBufferInfo = tempBufferInfos[i].data();
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        case VK_DESCRIPTOR_TYPE_SAMPLER: {
            if (w.pImageInfo) {
                tempImageInfos[i].assign(w.pImageInfo, w.pImageInfo + n);
                bool modified = false;
                for (uint32_t j = 0; j < n; j++) {
                    if ((w.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER) &&
                        tempImageInfos[i][j].imageView == VK_NULL_HANDLE) {
                        tempImageInfos[i][j].imageView =
                            dev->null_descriptors.null_image_view;
                        tempImageInfos[i][j].imageLayout =
                            VK_IMAGE_LAYOUT_GENERAL;
                        modified = true;
                    }
                    if ((w.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                         w.descriptorType ==
                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
                        tempImageInfos[i][j].sampler == VK_NULL_HANDLE) {
                        tempImageInfos[i][j].sampler =
                            dev->null_descriptors.null_sampler;
                        modified = true;
                    }
                }
                if (modified)
                    writes[i].pImageInfo = tempImageInfos[i].data();
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            if (w.pTexelBufferView) {
                tempTexelViews[i].assign(w.pTexelBufferView,
                                         w.pTexelBufferView + n);
                bool modified = false;
                for (uint32_t j = 0; j < n; j++) {
                    if (tempTexelViews[i][j] == VK_NULL_HANDLE) {
                        tempTexelViews[i][j] =
                            dev->null_descriptors.null_buffer_view;
                        modified = true;
                    }
                }
                if (modified)
                    writes[i].pTexelBufferView = tempTexelViews[i].data();
            }
            break;
        }
        default:
            break;
        }
    }

    dev->table.UpdateDescriptorSets(device, descriptorWriteCount, writes.data(),
                                    descriptorCopyCount, pDescriptorCopies);
}

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_UpdateDescriptorSetWithTemplate(
    VkDevice device, VkDescriptorSet descriptorSet,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData) {

    struct device *dev = get_device(device);
    if (!dev)
        return;

    auto *templ = get_descriptor_update_template(descriptorUpdateTemplate);
    if (!dev->emulate_null_descriptor || !templ || templ->entries.empty()) {
        dev->table.UpdateDescriptorSetWithTemplate(
            device, descriptorSet, descriptorUpdateTemplate, pData);
        return;
    }

    size_t max_payload_size = 0;
    for (const auto &entry : templ->entries) {
        size_t element_size = 0;
        switch (entry.descriptorType) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            element_size = sizeof(VkDescriptorImageInfo);
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            element_size = sizeof(VkDescriptorBufferInfo);
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            element_size = sizeof(VkBufferView);
            break;
        default:
            break;
        }
        if (element_size > 0) {
            size_t entry_end = entry.offset +
                               (entry.descriptorCount - 1) * entry.stride +
                               element_size;
            if (entry_end > max_payload_size)
                max_payload_size = entry_end;
        }
    }

    if (max_payload_size == 0) {
        dev->table.UpdateDescriptorSetWithTemplate(
            device, descriptorSet, descriptorUpdateTemplate, pData);
        return;
    }

    std::vector<uint8_t> temp_payload(max_payload_size);
    std::memcpy(temp_payload.data(), pData, max_payload_size);

    for (const auto &entry : templ->entries) {
        for (uint32_t j = 0; j < entry.descriptorCount; ++j) {
            uint8_t *elem_ptr =
                temp_payload.data() + entry.offset + (j * entry.stride);
            switch (entry.descriptorType) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
                auto *buf_info =
                    reinterpret_cast<VkDescriptorBufferInfo *>(elem_ptr);
                if (buf_info->buffer == VK_NULL_HANDLE) {
                    buf_info->buffer = dev->null_descriptors.null_buffer;
                    buf_info->offset = 0;
                    buf_info->range = VK_WHOLE_SIZE;
                }
                break;
            }
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
                auto *img_info =
                    reinterpret_cast<VkDescriptorImageInfo *>(elem_ptr);
                if (img_info->imageView == VK_NULL_HANDLE) {
                    img_info->imageView = dev->null_descriptors.null_image_view;
                    img_info->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                }
                break;
            }
            case VK_DESCRIPTOR_TYPE_SAMPLER: {
                auto *img_info =
                    reinterpret_cast<VkDescriptorImageInfo *>(elem_ptr);
                if (img_info->sampler == VK_NULL_HANDLE) {
                    img_info->sampler = dev->null_descriptors.null_sampler;
                }
                break;
            }
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                auto *img_info =
                    reinterpret_cast<VkDescriptorImageInfo *>(elem_ptr);
                if (img_info->imageView == VK_NULL_HANDLE) {
                    img_info->imageView = dev->null_descriptors.null_image_view;
                    img_info->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                }
                if (img_info->sampler == VK_NULL_HANDLE) {
                    img_info->sampler = dev->null_descriptors.null_sampler;
                }
                break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
                auto *view_info = reinterpret_cast<VkBufferView *>(elem_ptr);
                if (*view_info == VK_NULL_HANDLE) {
                    *view_info = dev->null_descriptors.null_buffer_view;
                }
                break;
            }
            default:
                break;
            }
        }
    }

    dev->table.UpdateDescriptorSetWithTemplate(
        device, descriptorSet, descriptorUpdateTemplate, temp_payload.data());
}
