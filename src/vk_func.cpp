#include "vk_func.hpp"

#include "logger.hpp"
#include "vulkan/vk_layer.h"

#include <vulkan/vulkan.h>

void init_dispatch_table(PFN_vkGetDeviceProcAddr gdpa, VkDevice device,
                         VkLayerDispatchTable &table) {
    // Sync this with vulkan/vk_layer.h
#define GET_DEVICE_PROC(func_name)                                             \
    table.func_name = reinterpret_cast<decltype(table.func_name)>(             \
        gdpa(device, "vk" #func_name));                                        \
    if (!table.func_name) {                                                    \
        table.func_name = reinterpret_cast<decltype(table.func_name)>(         \
            gdpa(device, "vk" #func_name "KHR"));                              \
        if (!table.func_name) {                                                \
            Logger::log("info", "Dispatch not found: vk" #func_name            \
                                " / vk" #func_name "KHR");                     \
        }                                                                      \
    }

    GET_DEVICE_PROC(GetDeviceProcAddr);
    GET_DEVICE_PROC(DestroyDevice);
    GET_DEVICE_PROC(GetDeviceQueue);
    GET_DEVICE_PROC(QueueSubmit);
    GET_DEVICE_PROC(QueueSubmit2);
    GET_DEVICE_PROC(QueueWaitIdle);
    GET_DEVICE_PROC(DeviceWaitIdle);
    GET_DEVICE_PROC(AllocateMemory);
    GET_DEVICE_PROC(FreeMemory);
    GET_DEVICE_PROC(MapMemory);
    GET_DEVICE_PROC(UnmapMemory);
    GET_DEVICE_PROC(FlushMappedMemoryRanges);
    GET_DEVICE_PROC(InvalidateMappedMemoryRanges);
    GET_DEVICE_PROC(GetDeviceMemoryCommitment);
    GET_DEVICE_PROC(GetImageSparseMemoryRequirements);
    GET_DEVICE_PROC(GetImageMemoryRequirements);
    GET_DEVICE_PROC(GetBufferMemoryRequirements);
    GET_DEVICE_PROC(BindImageMemory);
    GET_DEVICE_PROC(BindBufferMemory);
    GET_DEVICE_PROC(BindBufferMemory2);
    GET_DEVICE_PROC(QueueBindSparse);
    GET_DEVICE_PROC(CreateFence);
    GET_DEVICE_PROC(DestroyFence);
    GET_DEVICE_PROC(GetFenceStatus);
    GET_DEVICE_PROC(ResetFences);
    GET_DEVICE_PROC(WaitForFences);
    GET_DEVICE_PROC(CreateSemaphore);
    GET_DEVICE_PROC(DestroySemaphore);
    GET_DEVICE_PROC(CreateEvent);
    GET_DEVICE_PROC(DestroyEvent);
    GET_DEVICE_PROC(GetEventStatus);
    GET_DEVICE_PROC(SetEvent);
    GET_DEVICE_PROC(ResetEvent);
    GET_DEVICE_PROC(CreateQueryPool);
    GET_DEVICE_PROC(DestroyQueryPool);
    GET_DEVICE_PROC(GetQueryPoolResults);
    GET_DEVICE_PROC(CreateBuffer);
    GET_DEVICE_PROC(DestroyBuffer);
    GET_DEVICE_PROC(CreateBufferView);
    GET_DEVICE_PROC(DestroyBufferView);
    GET_DEVICE_PROC(CreateImage);
    GET_DEVICE_PROC(DestroyImage);
    GET_DEVICE_PROC(GetImageSubresourceLayout);
    GET_DEVICE_PROC(CreateImageView);
    GET_DEVICE_PROC(DestroyImageView);
    GET_DEVICE_PROC(CreateShaderModule);
    GET_DEVICE_PROC(DestroyShaderModule);
    GET_DEVICE_PROC(CreatePipelineCache);
    GET_DEVICE_PROC(DestroyPipelineCache);
    GET_DEVICE_PROC(GetPipelineCacheData);
    GET_DEVICE_PROC(MergePipelineCaches);
    GET_DEVICE_PROC(CreateGraphicsPipelines);
    GET_DEVICE_PROC(CreateComputePipelines);
    GET_DEVICE_PROC(DestroyPipeline);
    GET_DEVICE_PROC(CreatePipelineLayout);
    GET_DEVICE_PROC(DestroyPipelineLayout);
    GET_DEVICE_PROC(CreateSampler);
    GET_DEVICE_PROC(DestroySampler);
    GET_DEVICE_PROC(CreateDescriptorSetLayout);
    GET_DEVICE_PROC(DestroyDescriptorSetLayout);
    GET_DEVICE_PROC(CreateDescriptorPool);
    GET_DEVICE_PROC(DestroyDescriptorPool);
    GET_DEVICE_PROC(ResetDescriptorPool);
    GET_DEVICE_PROC(AllocateDescriptorSets);
    GET_DEVICE_PROC(FreeDescriptorSets);
    GET_DEVICE_PROC(UpdateDescriptorSets);
    GET_DEVICE_PROC(CreateFramebuffer);
    GET_DEVICE_PROC(DestroyFramebuffer);
    GET_DEVICE_PROC(CreateRenderPass);
    GET_DEVICE_PROC(DestroyRenderPass);
    GET_DEVICE_PROC(GetRenderAreaGranularity);
    GET_DEVICE_PROC(CreateCommandPool);
    GET_DEVICE_PROC(DestroyCommandPool);
    GET_DEVICE_PROC(ResetCommandPool);
    GET_DEVICE_PROC(AllocateCommandBuffers);
    GET_DEVICE_PROC(FreeCommandBuffers);
    GET_DEVICE_PROC(BeginCommandBuffer);
    GET_DEVICE_PROC(EndCommandBuffer);
    GET_DEVICE_PROC(ResetCommandBuffer);
    GET_DEVICE_PROC(CmdBindPipeline);
    GET_DEVICE_PROC(CmdBindDescriptorSets);
    GET_DEVICE_PROC(CmdBindDescriptorSets2);
    GET_DEVICE_PROC(CmdBindVertexBuffers);
    GET_DEVICE_PROC(CmdBindIndexBuffer);
    GET_DEVICE_PROC(CmdSetViewport);
    GET_DEVICE_PROC(CmdSetScissor);
    GET_DEVICE_PROC(CmdSetLineWidth);
    GET_DEVICE_PROC(CmdSetDepthBias);
    GET_DEVICE_PROC(CmdSetBlendConstants);
    GET_DEVICE_PROC(CmdSetDepthBounds);
    GET_DEVICE_PROC(CmdSetStencilCompareMask);
    GET_DEVICE_PROC(CmdSetStencilWriteMask);
    GET_DEVICE_PROC(CmdSetStencilReference);
    GET_DEVICE_PROC(CmdDraw);
    GET_DEVICE_PROC(CmdDrawIndexed);
    GET_DEVICE_PROC(CmdDrawIndirect);
    GET_DEVICE_PROC(CmdDrawIndexedIndirect);
#ifdef ENABLE_COMPUTE_TRACKING
    GET_DEVICE_PROC(CmdDispatch);
#else
    table.CmdDispatch = [](auto... args) {
        Logger::log("error", "vkCmdDispatch called without "
                             "ENABLE_COMPUTE_TRACKING=1, FAILING NOW");
        abort();
    };
#endif
    GET_DEVICE_PROC(CmdDispatchIndirect);
    GET_DEVICE_PROC(CmdCopyBuffer);
    GET_DEVICE_PROC(CmdCopyBuffer2);
    GET_DEVICE_PROC(CmdCopyImage);
    GET_DEVICE_PROC(CmdCopyImage2);
    GET_DEVICE_PROC(CmdBlitImage);
    GET_DEVICE_PROC(CmdCopyBufferToImage);
    GET_DEVICE_PROC(CmdCopyBufferToImage2);
    GET_DEVICE_PROC(CmdCopyImageToBuffer);
    GET_DEVICE_PROC(CmdUpdateBuffer);
    GET_DEVICE_PROC(CmdFillBuffer);
    GET_DEVICE_PROC(CmdClearColorImage);
    GET_DEVICE_PROC(CmdClearDepthStencilImage);
    GET_DEVICE_PROC(CmdClearAttachments);
    GET_DEVICE_PROC(CmdResolveImage);
    GET_DEVICE_PROC(CmdSetEvent);
    GET_DEVICE_PROC(CmdResetEvent);
    GET_DEVICE_PROC(CmdWaitEvents);
    GET_DEVICE_PROC(CmdPipelineBarrier);
    GET_DEVICE_PROC(CmdBeginQuery);
    GET_DEVICE_PROC(CmdEndQuery);
    GET_DEVICE_PROC(CmdResetQueryPool);
    GET_DEVICE_PROC(CmdWriteTimestamp);
    GET_DEVICE_PROC(CmdCopyQueryPoolResults);
    GET_DEVICE_PROC(CmdPushConstants);
    GET_DEVICE_PROC(CmdPushConstants2);
    GET_DEVICE_PROC(CmdBeginRenderPass);
    GET_DEVICE_PROC(CmdNextSubpass);
    GET_DEVICE_PROC(CmdEndRenderPass);
    GET_DEVICE_PROC(CmdExecuteCommands);
    GET_DEVICE_PROC(CreateSwapchainKHR);
    GET_DEVICE_PROC(DestroySwapchainKHR);
    GET_DEVICE_PROC(GetSwapchainImagesKHR);
    GET_DEVICE_PROC(AcquireNextImageKHR);
    GET_DEVICE_PROC(QueuePresentKHR);
    GET_DEVICE_PROC(CmdDrawIndirectCountAMD);
    GET_DEVICE_PROC(CmdDrawIndexedIndirectCountAMD);
#ifdef VK_USE_PLATFORM_WIN32_KHR
    GET_DEVICE_PROC(GetMemoryWin32HandleNV);
#endif
    GET_DEVICE_PROC(CreateSharedSwapchainsKHR);
    GET_DEVICE_PROC(DebugMarkerSetObjectTagEXT);
    GET_DEVICE_PROC(DebugMarkerSetObjectNameEXT);
    GET_DEVICE_PROC(CmdDebugMarkerBeginEXT);
    GET_DEVICE_PROC(CmdDebugMarkerEndEXT);
    GET_DEVICE_PROC(CmdDebugMarkerInsertEXT);
    GET_DEVICE_PROC(GetDeviceFaultInfoEXT);
    GET_DEVICE_PROC(DeviceSetApiDumpState);
    GET_DEVICE_PROC(GetBufferDeviceAddress);
    GET_DEVICE_PROC(CreateDescriptorUpdateTemplate);
    GET_DEVICE_PROC(DestroyDescriptorUpdateTemplate);
    GET_DEVICE_PROC(UpdateDescriptorSetWithTemplate);
    GET_DEVICE_PROC(CmdPushDescriptorSetKHR);
    GET_DEVICE_PROC(CmdPushDescriptorSet);
    GET_DEVICE_PROC(CmdPushDescriptorSetWithTemplateKHR);
    GET_DEVICE_PROC(CmdPushDescriptorSetWithTemplate);
    GET_DEVICE_PROC(CmdBindVertexBuffers);
    GET_DEVICE_PROC(CmdBindVertexBuffers2);
    if (!table.CmdBindVertexBuffers2) {
        table.CmdBindVertexBuffers2 =
            reinterpret_cast<decltype(table.CmdBindVertexBuffers2)>(
                gdpa(device, "vkCmdBindVertexBuffers2EXT"));
    }

#undef GET_DEVICE_PROC
}
