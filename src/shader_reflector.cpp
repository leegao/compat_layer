#include "descriptors.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "pipelines.hpp"
#include "shader_reflector.hpp"
#include "spirv_reflect/spirv_reflect.h"
#include <mutex>
#include <vulkan/vulkan.h>

std::shared_mutex shaderModulesLock;
std::unordered_map<VkShaderModule, ShaderModuleInfo> shaderModulesMap;

void TrackShader(struct device *dev, VkShaderModule shaderModule,
                 const VkShaderModuleCreateInfo *pCreateInfo) {
    if (!dev->emulate_precise_null_descriptor) {
        return;
    }

    SpvReflectShaderModule reflectModule;
    auto result = spvReflectCreateShaderModule(
        pCreateInfo->codeSize, pCreateInfo->pCode, &reflectModule);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        Logger::log("error", "spvReflectCreateShaderModule failed with %d",
                    result);
        return;
    }

    uint32_t binding_count = 0;
    result = spvReflectEnumerateDescriptorBindings(&reflectModule,
                                                   &binding_count, nullptr);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        spvReflectDestroyShaderModule(&reflectModule);
        return;
    }

    std::vector<SpvReflectDescriptorBinding *> bindings(binding_count);
    result = spvReflectEnumerateDescriptorBindings(
        &reflectModule, &binding_count, bindings.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        spvReflectDestroyShaderModule(&reflectModule);
        return;
    }

    ShaderModuleInfo info;
    for (uint32_t i = 0; i < binding_count; ++i) {
        const auto *binding = bindings[i];
        DescriptorBindingInfo descriptorBindingInfo;
        descriptorBindingInfo.set = binding->set;
        descriptorBindingInfo.binding = binding->binding;

        descriptorBindingInfo.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
        if (binding->descriptor_type ==
                SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            binding->descriptor_type ==
                SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
            binding->descriptor_type ==
                SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            binding->descriptor_type ==
                SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {

            switch (binding->image.dim) {
            case SpvDim1D:
                descriptorBindingInfo.imageViewType =
                    binding->image.arrayed ? VK_IMAGE_VIEW_TYPE_1D_ARRAY
                                           : VK_IMAGE_VIEW_TYPE_1D;
                break;
            case SpvDim2D:
                descriptorBindingInfo.imageViewType =
                    binding->image.arrayed ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                           : VK_IMAGE_VIEW_TYPE_2D;
                break;
            case SpvDim3D:
                descriptorBindingInfo.imageViewType = VK_IMAGE_VIEW_TYPE_3D;
                break;
            case SpvDimCube:
                descriptorBindingInfo.imageViewType =
                    binding->image.arrayed ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
                                           : VK_IMAGE_VIEW_TYPE_CUBE;
                break;
            default:
                descriptorBindingInfo.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
                break;
            }
        }

        descriptorBindingInfo.format = VK_FORMAT_UNDEFINED;
        switch (binding->image.image_format) {
        case SpvImageFormatRgba32f:
            descriptorBindingInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            break;
        case SpvImageFormatRgba16f:
            descriptorBindingInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            break;
        case SpvImageFormatR32f:
            descriptorBindingInfo.format = VK_FORMAT_R32_SFLOAT;
            break;
        case SpvImageFormatRgba8:
            descriptorBindingInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        case SpvImageFormatRgba8Snorm:
            descriptorBindingInfo.format = VK_FORMAT_R8G8B8A8_SNORM;
            break;
        case SpvImageFormatRg32f:
            descriptorBindingInfo.format = VK_FORMAT_R32G32_SFLOAT;
            break;
        case SpvImageFormatRg16f:
            descriptorBindingInfo.format = VK_FORMAT_R16G16_SFLOAT;
            break;
        case SpvImageFormatR11fG11fB10f:
            descriptorBindingInfo.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            break;
        case SpvImageFormatR16f:
            descriptorBindingInfo.format = VK_FORMAT_R16_SFLOAT;
            break;
        case SpvImageFormatRgba16:
            descriptorBindingInfo.format = VK_FORMAT_R16G16B16A16_UNORM;
            break;
        case SpvImageFormatRgb10A2:
            descriptorBindingInfo.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            break;
        case SpvImageFormatRg16:
            descriptorBindingInfo.format = VK_FORMAT_R16G16_UNORM;
            break;
        case SpvImageFormatRg8:
            descriptorBindingInfo.format = VK_FORMAT_R8G8_UNORM;
            break;
        case SpvImageFormatR16:
            descriptorBindingInfo.format = VK_FORMAT_R16_UNORM;
            break;
        case SpvImageFormatR8:
            descriptorBindingInfo.format = VK_FORMAT_R8_UNORM;
            break;
        case SpvImageFormatRgba16Snorm:
            descriptorBindingInfo.format = VK_FORMAT_R16G16B16A16_SNORM;
            break;
        case SpvImageFormatRg16Snorm:
            descriptorBindingInfo.format = VK_FORMAT_R16G16_SNORM;
            break;
        case SpvImageFormatRg8Snorm:
            descriptorBindingInfo.format = VK_FORMAT_R8G8_SNORM;
            break;
        case SpvImageFormatR16Snorm:
            descriptorBindingInfo.format = VK_FORMAT_R16_SNORM;
            break;
        case SpvImageFormatR8Snorm:
            descriptorBindingInfo.format = VK_FORMAT_R8_SNORM;
            break;
        case SpvImageFormatRgba32i:
            descriptorBindingInfo.format = VK_FORMAT_R32G32B32A32_SINT;
            break;
        case SpvImageFormatRgba16i:
            descriptorBindingInfo.format = VK_FORMAT_R16G16B16A16_SINT;
            break;
        case SpvImageFormatRgba8i:
            descriptorBindingInfo.format = VK_FORMAT_R8G8B8A8_SINT;
            break;
        case SpvImageFormatR32i:
            descriptorBindingInfo.format = VK_FORMAT_R32_SINT;
            break;
        case SpvImageFormatRg32i:
            descriptorBindingInfo.format = VK_FORMAT_R32G32_SINT;
            break;
        case SpvImageFormatRg16i:
            descriptorBindingInfo.format = VK_FORMAT_R16G16_SINT;
            break;
        case SpvImageFormatRg8i:
            descriptorBindingInfo.format = VK_FORMAT_R8G8_SINT;
            break;
        case SpvImageFormatR16i:
            descriptorBindingInfo.format = VK_FORMAT_R16_SINT;
            break;
        case SpvImageFormatR8i:
            descriptorBindingInfo.format = VK_FORMAT_R8_SINT;
            break;
        case SpvImageFormatRgba32ui:
            descriptorBindingInfo.format = VK_FORMAT_R32G32B32A32_UINT;
            break;
        case SpvImageFormatRgba16ui:
            descriptorBindingInfo.format = VK_FORMAT_R16G16B16A16_UINT;
            break;
        case SpvImageFormatRgba8ui:
            descriptorBindingInfo.format = VK_FORMAT_R8G8B8A8_UINT;
            break;
        case SpvImageFormatR32ui:
            descriptorBindingInfo.format = VK_FORMAT_R32_UINT;
            break;
        case SpvImageFormatRg32ui:
            descriptorBindingInfo.format = VK_FORMAT_R32G32_UINT;
            break;
        case SpvImageFormatRg16ui:
            descriptorBindingInfo.format = VK_FORMAT_R16G16_UINT;
            break;
        case SpvImageFormatRg8ui:
            descriptorBindingInfo.format = VK_FORMAT_R8G8_UINT;
            break;
        case SpvImageFormatR16ui:
            descriptorBindingInfo.format = VK_FORMAT_R16_UINT;
            break;
        case SpvImageFormatR8ui:
            descriptorBindingInfo.format = VK_FORMAT_R8_UINT;
            break;
        default:
            descriptorBindingInfo.format = VK_FORMAT_UNDEFINED;
            break;
        }

        info.bindings.push_back(descriptorBindingInfo);
    }

    spvReflectDestroyShaderModule(&reflectModule);

    {
        std::unique_lock l(shaderModulesLock); // writer
        shaderModulesMap[shaderModule] = std::move(info);
    }
}

void UntrackShader(VkShaderModule shaderModule) {
    std::unique_lock l(shaderModulesLock); // writer
    shaderModulesMap.erase(shaderModule);
}

void TrackPipelineDescriptorLayoutBindingTypes(
    struct device *dev, VkPipelineLayout layoutHandle, uint32_t stageCount,
    const VkPipelineShaderStageCreateInfo *pStages) {
    auto *pipelineLayout = ({
        std::shared_lock l(pipelineLayoutsLock); // reader
        get_pipeline_layout(layoutHandle);
    });

    if (!pipelineLayout) {
        return;
    }

    for (uint32_t s = 0; s < stageCount; ++s) {
        VkShaderModule module = pStages[s].module;
        const auto &shaderModuleInfo = ({
            std::shared_lock l(shaderModulesLock); // reader
            auto it = shaderModulesMap.find(module);
            if (it == shaderModulesMap.end())
                continue;
            it->second;
        });

        for (const auto &bindingInfo : shaderModuleInfo.bindings) {
            if (bindingInfo.set >= pipelineLayout->setLayouts.size()) {
                continue;
            }
            VkDescriptorSetLayout layoutHandle =
                pipelineLayout->setLayouts[bindingInfo.set];
            auto *descriptorSetLayout = ({
                std::shared_lock l(descriptorSetLayoutsLock);
                get_descriptor_set_layout(layoutHandle);
            });
        }
    }
}
