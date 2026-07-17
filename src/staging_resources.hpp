#ifndef __STAGING_RESOURCES_HPP
#define __STAGING_RESOURCES_HPP

#include "buffer.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

struct command_buffer;

struct TimestampQuery {
    std::string label;
    VkFormat format;
    uint64_t textureSize;
    size_t poolIndex;
    uint32_t startQueryId;
    uint32_t endQueryId;
};

struct QueryPoolBlock {
    VkQueryPool handle;
    uint32_t allocatedQueries;
};

class ScopedTimestampQuery {
  public:
    explicit ScopedTimestampQuery(struct command_buffer *cb,
                                  std::string_view label, VkFormat format,
                                  uint64_t texture_size, VkQueryPool queryPool,
                                  uint32_t startQueryId,
                                  VkPipelineStageFlagBits startStage,
                                  VkPipelineStageFlagBits endStage)
        : m_cb(cb), m_label(label), m_format(format),
          m_textureSize(texture_size), m_queryPool(queryPool),
          m_startQueryId(startQueryId), m_startStage(startStage),
          m_endStage(endStage) {
        if (m_queryPool != VK_NULL_HANDLE) {
            Start();
        }
    }

    ~ScopedTimestampQuery() {
        if (m_queryPool != VK_NULL_HANDLE) {
            End();
        }
    }

    ScopedTimestampQuery(const ScopedTimestampQuery &) = delete;
    ScopedTimestampQuery &operator=(const ScopedTimestampQuery &) = delete;

    ScopedTimestampQuery(ScopedTimestampQuery &&other) noexcept
        : m_cb(other.m_cb), m_label(other.m_label), m_format(other.m_format),
          m_textureSize(other.m_textureSize), m_queryPool(other.m_queryPool),
          m_startQueryId(other.m_startQueryId),
          m_startStage(other.m_startStage), m_endStage(other.m_endStage) {
        other.Reset();
    }

    ScopedTimestampQuery &operator=(ScopedTimestampQuery &&other) noexcept {
        if (this != &other) {
            m_cb = other.m_cb;
            m_label = other.m_label;
            m_format = other.m_format;
            m_textureSize = other.m_textureSize;
            m_queryPool = other.m_queryPool;
            m_startQueryId = other.m_startQueryId;
            m_startStage = other.m_startStage;
            m_endStage = other.m_endStage;

            other.Reset();
        }
        return *this;
    }

  private:
    void Start();
    void End();

    void Reset() {
        m_cb = nullptr;
        m_label = "";
        m_queryPool = VK_NULL_HANDLE;
        m_startQueryId = UINT32_MAX;
    }

    struct command_buffer *m_cb;
    std::string_view m_label;
    VkFormat m_format;
    uint64_t m_textureSize;
    VkQueryPool m_queryPool;
    uint32_t m_startQueryId;
    VkPipelineStageFlagBits m_startStage;
    VkPipelineStageFlagBits m_endStage;
};

struct StagingResources {
  public:
    explicit StagingResources(VkDevice device) : device(device) {}
    ~StagingResources();

    std::pair<VkSemaphore, VkFence> MakeFence();
    bool IsCompleted() const { return has_completed; }
    bool IsEmpty() const {
        return stagingBuffers.empty() && stagingImageViews.empty() &&
               trackedQueries.empty() && descriptorSets.empty() &&
               semaphore == VK_NULL_HANDLE && completed == VK_NULL_HANDLE;
    }
    void WaitForCompletion();
    void Cleanup();
    void AddStagingBuffer(VkBuffer buf) { stagingBuffers.push_back(buf); }
    void AddStagingImageView(VkImageView view) {
        stagingImageViews.push_back(view);
    }
    void AddDescriptorSet(VkDescriptorPool pool, VkDescriptorSet set) {
        descriptorSets.push_back({pool, set});
    }
    int Size() const { return stagingBuffers.size(); }
    int MemoryUsage() const { return 0; }
    ScopedTimestampQuery MakeScopedTimestampQuery(
        struct command_buffer *cb, const std::string &label,
        VkFormat format = VK_FORMAT_UNDEFINED, uint64_t texture_size = 0,
        VkPipelineStageFlagBits startStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VkPipelineStageFlagBits endStage =
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    int id = 0;
    int64_t timestamp = 0;

  private:
    static const uint32_t kPoolBlockSize = 128;

    VkFence completed = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VkDevice device;

    bool freed = false;
    bool has_completed = false;
    std::vector<VkBuffer> stagingBuffers;
    std::vector<VkImageView> stagingImageViews;
    std::vector<std::pair<VkDescriptorPool, VkDescriptorSet>> descriptorSets;
    std::vector<QueryPoolBlock> queryPools;
    std::vector<TimestampQuery> trackedQueries;
};

#endif
