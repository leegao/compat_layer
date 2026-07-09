#ifndef __VK_FUNC_HPP
#define __VK_FUNC_HPP

#include "vulkan/vk_layer.h"
#include <vulkan/vulkan.h>

void init_dispatch_table(PFN_vkGetDeviceProcAddr, VkDevice,
                         VkLayerDispatchTable &);

extern "C" {

VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateImage(
    VkDevice device, const VkImageCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkImage *pImage);

VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateImageView(
    VkDevice device, const VkImageViewCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkImageView *pImageView);

void VKAPI_CALL DxvkMaliCompatLayer_DestroyImage(
    VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator);

VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateBuffer(
    VkDevice device, const VkBufferCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer);

VkResult VKAPI_CALL DxvkMaliCompatLayer_BindBufferMemory(
    VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
    VkDeviceSize memoryOffset);

VkResult VKAPI_CALL DxvkMaliCompatLayer_BindBufferMemory2(
    VkDevice device, uint32_t bindInfoCount,
    const VkBindBufferMemoryInfo *pBindInfos);

void VKAPI_CALL DxvkMaliCompatLayer_DestroyBuffer(
    VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *pAllocator);

VkResult VKAPI_CALL DxvkMaliCompatLayer_AllocateCommandBuffers(
    VkDevice device, const VkCommandBufferAllocateInfo *pAllocateInfo,
    VkCommandBuffer *pCommandBuffers);

void VKAPI_CALL DxvkMaliCompatLayer_FreeCommandBuffers(
    VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount,
    const VkCommandBuffer *pCommandBuffers);

VkResult VKAPI_CALL DxvkMaliCompatLayer_BeginCommandBuffer(
    VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *pBeginInfo);

VkResult VKAPI_CALL DxvkMaliCompatLayer_ResetCommandBuffer(
    VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags);

void VKAPI_CALL DxvkMaliCompatLayer_CmdBindPipeline(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipeline pipeline);

void VKAPI_CALL DxvkMaliCompatLayer_CmdBindDescriptorSets(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
    const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount,
    const uint32_t *pDynamicOffsets);

void VKAPI_CALL DxvkMaliCompatLayer_CmdBindDescriptorSets2(
    VkCommandBuffer commandBuffer,
    const VkBindDescriptorSetsInfo *pBindDescriptorSetsInfo);

void VKAPI_CALL DxvkMaliCompatLayer_CmdPushConstants(
    VkCommandBuffer commandBuffer, VkPipelineLayout layout,
    VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size,
    const void *pValues);

void VKAPI_CALL DxvkMaliCompatLayer_CmdPushConstants2(
    VkCommandBuffer commandBuffer,
    const VkPushConstantsInfo *pPushConstantsInfo);

void VKAPI_CALL DxvkMaliCompatLayer_CmdCopyBufferToImage(
    VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage,
    VkImageLayout dstImageLayout, uint32_t regionCount,
    const VkBufferImageCopy *pRegions);

void VKAPI_CALL DxvkMaliCompatLayer_CmdCopyBufferToImage2(
    VkCommandBuffer commandBuffer,
    const VkCopyBufferToImageInfo2 *pCopyBufferToImageInfo);

void VKAPI_CALL DxvkMaliCompatLayer_CmdCopyImage2(
    VkCommandBuffer commandBuffer, const VkCopyImageInfo2 *pCopyImageInfo);

void VKAPI_CALL DxvkMaliCompatLayer_CmdCopyImage(
    VkCommandBuffer commandBuffer, VkImage srcImage,
    VkImageLayout srcImageLayout, VkImage dstImage,
    VkImageLayout dstImageLayout, uint32_t regionCount,
    const VkImageCopy *pRegions);

void VKAPI_CALL DxvkMaliCompatLayer_CmdCopyBuffer(
    VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer,
    uint32_t regionCount, const VkBufferCopy *pRegions);

void VKAPI_CALL DxvkMaliCompatLayer_CmdCopyBuffer2(
    VkCommandBuffer commandBuffer, const VkCopyBufferInfo2 *pCopyBufferInfo);

void VKAPI_CALL DxvkMaliCompatLayer_CmdUpdateBuffer(
    VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset,
    VkDeviceSize dataSize, const void *pData);

void VKAPI_CALL DxvkMaliCompatLayer_CmdBindDescriptorBuffersEXT(
    VkCommandBuffer commandBuffer, uint32_t bufferCount,
    const VkDescriptorBufferBindingInfoEXT *pBindingInfos);

void VKAPI_CALL DxvkMaliCompatLayer_CmdSetDescriptorBufferOffsetsEXT(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount,
    const uint32_t *pBufferIndices, const VkDeviceSize *pOffsets);

void VKAPI_CALL DxvkMaliCompatLayer_CmdDraw(VkCommandBuffer commandBuffer,
                                              uint32_t vertexCount,
                                              uint32_t instanceCount,
                                              uint32_t firstVertex,
                                              uint32_t firstInstance);

void VKAPI_CALL DxvkMaliCompatLayer_CmdDrawIndexed(
    VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount,
    uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

void VKAPI_CALL DxvkMaliCompatLayer_CmdDrawIndirect(
    VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
    uint32_t drawCount, uint32_t stride);

void VKAPI_CALL DxvkMaliCompatLayer_CmdDrawIndexedIndirect(
    VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
    uint32_t drawCount, uint32_t stride);

void VKAPI_CALL DxvkMaliCompatLayer_CmdDispatch(VkCommandBuffer commandBuffer,
                                                  uint32_t groupCountX,
                                                  uint32_t groupCountY,
                                                  uint32_t groupCountZ);

void VKAPI_CALL DxvkMaliCompatLayer_CmdDispatchIndirect(
    VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset);

void VKAPI_CALL DxvkMaliCompatLayer_GetDeviceQueue(VkDevice device,
                                                     uint32_t queueFamilyIndex,
                                                     uint32_t queueIndex,
                                                     VkQueue *pQueue);

VkResult VKAPI_CALL DxvkMaliCompatLayer_QueueSubmit(
    VkQueue queue, uint32_t submitInfoCount, const VkSubmitInfo *pSubmitInfos,
    VkFence fence);

VkResult VKAPI_CALL DxvkMaliCompatLayer_QueueSubmit2(
    VkQueue queue, uint32_t submitInfoCount, const VkSubmitInfo2 *pSubmitInfos,
    VkFence fence);

VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateDescriptorSetLayout(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout);

void VKAPI_CALL DxvkMaliCompatLayer_DestroyDescriptorSetLayout(
    VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
    const VkAllocationCallbacks *pAllocator);

void VKAPI_CALL DxvkMaliCompatLayer_GetDescriptorSetLayoutSizeEXT(
    VkDevice device, VkDescriptorSetLayout layout, VkDeviceSize *pSize);

void VKAPI_CALL DxvkMaliCompatLayer_GetDescriptorSetLayoutBindingOffsetEXT(
    VkDevice device, VkDescriptorSetLayout layout, uint32_t binding,
    VkDeviceSize *pOffset);

VkResult VKAPI_CALL DxvkMaliCompatLayer_CreatePipelineLayout(
    VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout);

void VKAPI_CALL DxvkMaliCompatLayer_DestroyPipelineLayout(
    VkDevice device, VkPipelineLayout pipelineLayout,
    const VkAllocationCallbacks *pAllocator);

VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateGraphicsPipelines(
    VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
    const VkGraphicsPipelineCreateInfo *pCreateInfos,
    const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines);

VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateComputePipelines(
    VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
    const VkComputePipelineCreateInfo *pCreateInfos,
    const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines);

void VKAPI_CALL
DxvkMaliCompatLayer_DestroyPipeline(VkDevice device, VkPipeline pipeline,
                                      const VkAllocationCallbacks *pAllocator);

VkResult VKAPI_CALL DxvkMaliCompatLayer_MapMemory(
    VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
    VkDeviceSize size, VkMemoryMapFlags flags, void **ppData);

void VKAPI_CALL DxvkMaliCompatLayer_UnmapMemory(VkDevice device,
                                                  VkDeviceMemory memory);

void VKAPI_CALL
DxvkMaliCompatLayer_FreeMemory(VkDevice device, VkDeviceMemory memory,
                                 const VkAllocationCallbacks *pAllocator);

VkDeviceAddress VKAPI_CALL DxvkMaliCompatLayer_GetBufferDeviceAddress(
    VkDevice device, const VkBufferDeviceAddressInfo *pInfo);

VkResult VKAPI_CALL DxvkMaliCompatLayer_CreateFence(
    VkDevice device, const VkFenceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkFence *pFence);

VkResult VKAPI_CALL DxvkMaliCompatLayer_WaitForFences(VkDevice device,
                                                        uint32_t fenceCount,
                                                        const VkFence *pFences,
                                                        VkBool32 waitAll,
                                                        uint64_t timeout);

void VKAPI_CALL DxvkMaliCompatLayer_DestroyFence(
    VkDevice device, VkFence fence, const VkAllocationCallbacks *pAllocator);

void VKAPI_CALL DxvkMaliCompatLayer_GetDescriptorEXT(
    VkDevice device, const VkDescriptorGetInfoEXT *pDescriptorInfo,
    size_t dataSize, void *pDescriptor);

} // extern "C"
#endif
