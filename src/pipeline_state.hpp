#ifndef PIPELINE_STATE_HPP
#define PIPELINE_STATE_HPP

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

constexpr uint32_t kMaxTrackedDescriptorSets = 32;
constexpr uint32_t kMaxTrackedPushConstantBytes = 256;
constexpr uint32_t kMaxTrackedDynamicOffsets = 8;

constexpr uint32_t kMaxBoundSets = 64;

struct BoundDescriptorSet {
    bool valid = false;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    uint32_t dynamicOffsetCount = 0;
    std::array<uint32_t, kMaxTrackedDynamicOffsets> dynamicOffsets{};
};

struct PushConstantRange {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderStageFlags stageFlags = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
};

class ComputePipelineBindingsState {
  public:
    void reset() {
#ifdef ENABLE_COMPUTE_TRACKING
        pipelineBound = false;
        pipeline = VK_NULL_HANDLE;
        for (auto &s : sets)
            s = BoundDescriptorSet{};
        anyPushConstantsPushed = false;
        pushConstantRanges.clear();
#endif
    }

    void TrackPipeline(VkPipeline pipeline);

    void TrackPushConstants(VkPipelineLayout layout,
                            VkShaderStageFlags stageFlags, uint32_t offset,
                            uint32_t size, const void *pValues);

    void TrackDescriptorSets(VkPipelineLayout layout, uint32_t firstSet,
                             uint32_t descriptorSetCount,
                             const VkDescriptorSet *pDescriptorSets,
                             uint32_t dynamicOffsetCount,
                             const uint32_t *pDynamicOffsets);

    bool pipelineBound = false;
    VkPipeline pipeline = VK_NULL_HANDLE;
#ifdef ENABLE_COMPUTE_TRACKING
    std::array<BoundDescriptorSet, kMaxTrackedDescriptorSets> sets{};
    std::array<uint8_t, kMaxTrackedPushConstantBytes> pushConstantBytes{};
    bool anyPushConstantsPushed = false;
    std::vector<PushConstantRange> pushConstantRanges;
#endif
};

// Any injected compute pipeline must save and restore a snapshot of the
// pipeline state for the command buffer, this is done automatically by
// ScopedPipelineStateSnapshot
class ScopedPipelineStateSnapshot {
  public:
    explicit ScopedPipelineStateSnapshot(struct command_buffer *cb);
    ~ScopedPipelineStateSnapshot();

    // No copies or moves, this is purely a scope-guard.
    ScopedPipelineStateSnapshot(const ScopedPipelineStateSnapshot &) = delete;
    ScopedPipelineStateSnapshot &
    operator=(const ScopedPipelineStateSnapshot &) = delete;
    ScopedPipelineStateSnapshot(ScopedPipelineStateSnapshot &&other) = delete;
    ScopedPipelineStateSnapshot &
    operator=(ScopedPipelineStateSnapshot &&other) = delete;

  private:
    struct command_buffer *m_cb;
    ComputePipelineBindingsState m_snapshot;
};

#endif // PIPELINE_STATE_HPP
