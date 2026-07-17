#include "queue.hpp"

#include "command_buffer.hpp"
#include "logger.hpp"
#include "mali_gpu_profiler.hpp"
#include "staging_resources.hpp"
#include "vk_func.hpp"
#include <vulkan/vulkan.h>

#include <chrono>
#include <vulkan/vulkan.h>

std::unordered_map<VkQueue, std::shared_ptr<struct queue>> queuesMap;

struct queue *get_queue(VkQueue queue) {
    auto it = queuesMap.find(queue);
    if (it == queuesMap.end())
        return nullptr;

    return it->second.get();
}

VkResult SubmitOneShot(struct device *dev,
                       std::function<void(struct command_buffer *)> record_func,
                       const std::string_view shader_label, struct queue *q,
                       const std::vector<VkSemaphore> &waits,
                       const std::vector<VkSemaphore> &signals,
                       VkFence signal_fence, bool async) {
    VkResult result;
    const auto &table = dev->table;

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = q ? q->queueFamilyIndex : dev->queueFamilyIndex,
    };
    VkCommandPool command_pool;
    result = table.CreateCommandPool(dev->handle, &pool_info, nullptr,
                                     &command_pool);
    if (result != VK_SUCCESS) {
        Logger::log("error", "one_shot: vkCreateCommandPool failed, result: %d",
                    result);
        return result;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1};
    VkCommandBuffer commandBuffer;
    result = DxvkMaliCompatLayer_AllocateCommandBuffers(
        dev->handle, &alloc_info, &commandBuffer);
    if (result != VK_SUCCESS) {
        Logger::log("error",
                    "one_shot: vkAllocateCommandBuffers failed, result: %d",
                    result);
        table.DestroyCommandPool(dev->handle, command_pool, nullptr);
        return result;
    }

    auto cb = get_command_buffer(commandBuffer);
    if (!cb) {
        Logger::log("error", "one_shot: command buffer not found, handle: %p",
                    commandBuffer);
        return VK_ERROR_UNKNOWN;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    result = DxvkMaliCompatLayer_BeginCommandBuffer(commandBuffer, &begin_info);
    if (result != VK_SUCCESS) {
        Logger::log("error",
                    "one_shot: vkBeginCommandBuffer failed, result: %d",
                    result);
        goto cleanup_registry;
    }

    record_func(cb);

    result = table.EndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        Logger::log("error", "one_shot: vkEndCommandBuffer failed, result: %d",
                    result);
        goto cleanup_registry;
    }

    {
        VkQueue submitQueue = (q != nullptr) ? q->handle : dev->queue;
        std::vector<VkPipelineStageFlags> waitDstStageMasks(
            waits.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = static_cast<uint32_t>(waits.size()),
            .pWaitSemaphores = waits.empty() ? nullptr : waits.data(),
            .pWaitDstStageMask =
                waits.empty() ? nullptr : waitDstStageMasks.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = static_cast<uint32_t>(signals.size()),
            .pSignalSemaphores = signals.empty() ? nullptr : signals.data(),
        };

        VkFence fence;
        if (async) {
            // We don't need to wait for the submit to complete, so just signal
            // directly
            fence = signal_fence;
        } else {
            VkFenceCreateInfo fence_info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            result =
                table.CreateFence(dev->handle, &fence_info, nullptr, &fence);
            if (result != VK_SUCCESS) {
                Logger::log("error",
                            "Failed to create one-shot fence, result: %d",
                            result);
                goto cleanup_registry;
            }
        }

        result = DxvkMaliCompatLayer_QueueSubmit(submitQueue, 1, &submit_info,
                                                 fence);
        if (result != VK_SUCCESS) {
            Logger::log("error", "one_shot: vkQueueSubmit failed, result: %d",
                        result);
            goto cleanup_registry;
        }

        if (!async) {
            table.WaitForFences(dev->handle, 1, &fence, VK_TRUE, UINT64_MAX);
        }

        if (!async) {
            table.DestroyFence(dev->handle, fence, nullptr);
        }

        if (!async && signal_fence) {
            // Signal fence with an empty queue submit
            submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            };
            result =
                table.QueueSubmit(submitQueue, 1, &submit_info, signal_fence);
            if (result != VK_SUCCESS) {
                Logger::log("error",
                            "one_shot: vkQueueSubmit failed, result: %d",
                            result);
            }
        }
    }

cleanup_registry:
    DxvkMaliCompatLayer_FreeCommandBuffers(dev->handle, command_pool, 1,
                                           &commandBuffer);
    table.DestroyCommandPool(dev->handle, command_pool, nullptr);

    return result;
}

VkResult
SubmitOneShotAsync(struct device *dev,
                   std::function<void(struct command_buffer *)> record_func,
                   const std::string_view shader_label, struct queue *q,
                   const std::vector<VkSemaphore> &waits,
                   const std::vector<VkSemaphore> &signals,
                   VkFence signal_fence) {
    return SubmitOneShot(dev, record_func, shader_label, q, waits, signals,
                         signal_fence);
}

#define COPY_INFOS(infos, count, vec)                                          \
    if (other->infos && other->count > 0) {                                    \
        vec.assign(other->infos, other->infos + other->count);                 \
        this->infos = vec.data();                                              \
    } else {                                                                   \
        this->infos = nullptr;                                                 \
    }

// An updatable VkSubmitInfo wrapper that allows inline modifications
class VkSubmitInfoUpdater : public VkSubmitInfo {
  public:
    VkSubmitInfoUpdater() {
        *(VkSubmitInfo *)this = VkSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    }

    explicit VkSubmitInfoUpdater(const VkSubmitInfo *other) {
        if (!other)
            return;
        *(VkSubmitInfo *)this = *other;
        COPY_INFOS(pWaitSemaphores, waitSemaphoreCount, m_waitSemaphores);
        COPY_INFOS(pWaitDstStageMask, waitSemaphoreCount, m_waitDstStageMask);
        COPY_INFOS(pSignalSemaphores, signalSemaphoreCount, m_signalSemaphores);
    }

    void addSignalSemaphore(VkSemaphore semaphore) {
        m_signalSemaphores.push_back(semaphore);
        this->signalSemaphoreCount =
            static_cast<uint32_t>(m_signalSemaphores.size());
        this->pSignalSemaphores = m_signalSemaphores.data();
    }

    void addWaitSemaphore(
        VkSemaphore semaphore,
        VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) {
        m_waitSemaphores.push_back(semaphore);
        m_waitDstStageMask.push_back(flags);
        this->waitSemaphoreCount =
            static_cast<uint32_t>(m_waitSemaphores.size());
        this->pWaitSemaphores = m_waitSemaphores.data();
        this->pWaitDstStageMask = m_waitDstStageMask.data();
    }

  private:
    std::vector<VkSemaphore> m_waitSemaphores;
    std::vector<VkPipelineStageFlags> m_waitDstStageMask;
    std::vector<VkSemaphore> m_signalSemaphores;
};

class VkSubmitInfo2Updater : public VkSubmitInfo2 {
  public:
    VkSubmitInfo2Updater() {
        *(VkSubmitInfo2 *)this = VkSubmitInfo2{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    }

    explicit VkSubmitInfo2Updater(const VkSubmitInfo2 *other) {
        if (!other)
            return;
        *(VkSubmitInfo2 *)this = *other;
        COPY_INFOS(pWaitSemaphoreInfos, waitSemaphoreInfoCount,
                   m_waitSemaphoreInfos);
        COPY_INFOS(pSignalSemaphoreInfos, signalSemaphoreInfoCount,
                   m_signalSemaphoreInfos);
    }

    void addSignalSemaphore(VkSemaphore semaphore,
                            VkPipelineStageFlags2 stageMask =
                                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT) {
        VkSemaphoreSubmitInfo info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = semaphore,
            .stageMask = stageMask,
        };

        m_signalSemaphoreInfos.push_back(info);
        this->signalSemaphoreInfoCount =
            static_cast<uint32_t>(m_signalSemaphoreInfos.size());
        this->pSignalSemaphoreInfos = m_signalSemaphoreInfos.data();
    }

    void addWaitSemaphore(
        VkSemaphore semaphore,
        VkPipelineStageFlags2 stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT) {
        VkSemaphoreSubmitInfo info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = semaphore,
            .stageMask = stageMask,
        };

        m_waitSemaphoreInfos.push_back(info);
        this->waitSemaphoreInfoCount =
            static_cast<uint32_t>(m_waitSemaphoreInfos.size());
        this->pWaitSemaphoreInfos = m_waitSemaphoreInfos.data();
    }

  private:
    std::vector<VkSemaphoreSubmitInfo> m_waitSemaphoreInfos;
    std::vector<VkSemaphoreSubmitInfo> m_signalSemaphoreInfos;
};

VK_LAYER_EXPORT void VKAPI_CALL
DxvkMaliCompatLayer_GetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex,
                                   uint32_t queueIndex, VkQueue *pQueue) {
    scoped_lock l(global_lock);

    struct device *dev = get_device(device);
    dev->table.GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);

    auto queue = std::make_shared<struct queue>();
    queue->handle = *pQueue;
    queue->device = dev;
    queue->queueFamilyIndex = queueFamilyIndex;
    queue->queueIndex = queueIndex;

    queuesMap[*pQueue] = queue;
}

static std::atomic_int kStagingResourceCounter;

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_QueueSubmit(
    VkQueue queue, uint32_t submitInfoCount, const VkSubmitInfo *pSubmitInfos,
    VkFence fence) {
    struct queue *q;

    // This must be scoped until the QueueSubmit call is made
    std::vector<VkSubmitInfoUpdater> updaters(submitInfoCount);
    std::vector<std::pair<VkSemaphore, VkFence>> staging_fences;
    bool needsCleanup = false;
    {
        scoped_lock l(global_lock);
        q = get_queue(queue);

        for (int i = 0; i < submitInfoCount; i++) {
            updaters[i] = VkSubmitInfoUpdater(&pSubmitInfos[i]);
            for (int j = 0; j < pSubmitInfos[i].commandBufferCount; j++) {
                struct command_buffer *cb =
                    get_command_buffer(pSubmitInfos[i].pCommandBuffers[j]);
                auto stagingResources = std::move(cb->currentStagingResources);
                // In case this command buffer is reused, reset the staging
                // resources.
                cb->currentStagingResources =
                    std::make_unique<StagingResources>(q->device->handle);

                // Technically shouldn't be needed, but just in case some game
                // engine forgets to reset the command buffer
                cb->reset_compute_state();

                if (!stagingResources->IsEmpty()) {
                    needsCleanup = true;
                }

                std::pair<VkSemaphore, VkFence> staging_fence =
                    stagingResources->MakeFence();
                if (staging_fence.first != VK_NULL_HANDLE) {
                    // Signal the temp semaphore
                    updaters[i].addSignalSemaphore(staging_fence.first);
                    staging_fences.push_back(staging_fence);
                    stagingResources->id = kStagingResourceCounter++;
                    auto now = std::chrono::system_clock::now();
                    stagingResources->timestamp =
                        std::chrono::time_point_cast<std::chrono::milliseconds>(
                            now)
                            .time_since_epoch()
                            .count();
                    cb->device->stagingResourcesQueue.push_back(
                        std::move(stagingResources));
                }
            }
        }
    }

    q->device->hasCleanupWork.notify_one();

    if (q->device->sample_gpu_counters && needsCleanup) {
        get_mali_gpu_profiler().Start();
    }

    std::vector<VkSubmitInfo> submit_infos(updaters.begin(), updaters.end());
    VkResult result = q->device->table.QueueSubmit(queue, submitInfoCount,
                                                   submit_infos.data(), fence);
    if (result != VK_SUCCESS) {
        Logger::log("error", "QueueSubmit failed: %d", result);
        return result;
    }

    if (q->device->sample_gpu_counters && needsCleanup) {
        q->device->table.QueueWaitIdle(queue);
        get_mali_gpu_profiler().StopAndProcess("QueueSubmit");
    }

    // Submit an empty command buffer to queue the tmp sem and signal the
    // resource completed fence
    for (const auto &staging_fence : staging_fences) {
        VkSubmitInfoUpdater submit_info;
        submit_info.addWaitSemaphore(staging_fence.first);
        result = q->device->table.QueueSubmit(queue, 1, &submit_info,
                                              staging_fence.second);
        if (result != VK_SUCCESS) {
            Logger::log("error", "QueueSubmit of staging_fences failed: %d",
                        result);
            return result;
        }
    }

    return result;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL DxvkMaliCompatLayer_QueueSubmit2(
    VkQueue queue, uint32_t submitInfoCount, const VkSubmitInfo2 *pSubmitInfos,
    VkFence fence) {
    struct queue *q;
    // This must be scoped until the QueueSubmit call is made
    std::vector<VkSubmitInfo2Updater> updaters(submitInfoCount);
    std::vector<std::pair<VkSemaphore, VkFence>> staging_fences;
    {
        scoped_lock l(global_lock);

        q = get_queue(queue);
        for (int i = 0; i < submitInfoCount; i++) {
            updaters[i] = VkSubmitInfo2Updater(&pSubmitInfos[i]);
            for (int j = 0; j < pSubmitInfos[i].commandBufferInfoCount; j++) {
                struct command_buffer *cb = get_command_buffer(
                    pSubmitInfos[i].pCommandBufferInfos[j].commandBuffer);
                auto stagingResources = std::move(cb->currentStagingResources);
                // In case this command buffer is reused, reset the staging
                // resources.
                cb->currentStagingResources =
                    std::make_unique<StagingResources>(q->device->handle);

                // Technically shouldn't be needed, but just in case some game
                // engine forgets to reset the command buffer
                cb->reset_compute_state();

                std::pair<VkSemaphore, VkFence> staging_fence =
                    stagingResources->MakeFence();
                if (staging_fence.first != VK_NULL_HANDLE) {
                    // Signal the temp semaphore
                    updaters[i].addSignalSemaphore(staging_fence.first);
                    staging_fences.push_back(staging_fence);
                    stagingResources->id = kStagingResourceCounter++;
                    auto now = std::chrono::system_clock::now();
                    stagingResources->timestamp =
                        std::chrono::time_point_cast<std::chrono::milliseconds>(
                            now)
                            .time_since_epoch()
                            .count();
                    cb->device->stagingResourcesQueue.push_back(
                        std::move(stagingResources));
                }
            }
        }
    }

    q->device->hasCleanupWork.notify_one();

    std::vector<VkSubmitInfo2> submit_infos(updaters.begin(), updaters.end());
    VkResult result = q->device->table.QueueSubmit2(queue, submitInfoCount,
                                                    submit_infos.data(), fence);
    if (result != VK_SUCCESS) {
        Logger::log("error", "QueueSubmit2 failed: %d", result);
        return result;
    }

    // Submit an empty command buffer to queue the tmp sem and signal the
    // resource completed fence
    for (const auto &staging_fence : staging_fences) {
        VkSubmitInfoUpdater submit_info;
        submit_info.addWaitSemaphore(staging_fence.first);
        result = q->device->table.QueueSubmit(queue, 1, &submit_info,
                                              staging_fence.second);
        if (result != VK_SUCCESS) {
            Logger::log("error", "QueueSubmit of staging_fences failed: %d",
                        result);
            return result;
        }
    }

    return result;
}
