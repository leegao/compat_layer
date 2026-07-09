#include "staging_resources.hpp"

#include "buffer.hpp"
#include "command_buffer.hpp"
#include "layer.hpp"
#include "logger.hpp"

#include <cstdint>
#include <map>

void LogDeviceFault(struct device *dev, const char *call) {
    Logger::log("error",
                "FATAL: %s failed with a GPU fault, the game will now crash",
                call);

    if (!dev->table.GetDeviceFaultInfoEXT) {
        Logger::log("error", "+ vkGetDeviceFaultInfoEXT is not available, "
                             "cannot dump vendor specific fault info");
        return;
    }

    VkDeviceFaultCountsEXT fault_counts = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT,
    };
    VkResult result =
        dev->table.GetDeviceFaultInfoEXT(dev->handle, &fault_counts, NULL);
    if (result != VK_SUCCESS) {
        return;
    }

    if (fault_counts.addressInfoCount == 0 &&
        fault_counts.vendorInfoCount == 0 &&
        fault_counts.vendorBinarySize == 0) {
        Logger::log(
            "error",
            "+ Device lost, but no fault info was recorded by the driver.");
        return;
    }

    VkDeviceFaultInfoEXT fault_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT,
    };

    std::vector<VkDeviceFaultAddressInfoEXT> addressInfos(
        fault_counts.addressInfoCount);
    std::vector<VkDeviceFaultVendorInfoEXT> vendorInfos(
        fault_counts.vendorInfoCount);
    std::vector<char> vendorBinaryData(fault_counts.vendorBinarySize);

    fault_info.pAddressInfos = addressInfos.data();
    fault_info.pVendorInfos = vendorInfos.data();
    fault_info.pVendorBinaryData = vendorBinaryData.data();

    result = dev->table.GetDeviceFaultInfoEXT(dev->handle, &fault_counts,
                                              &fault_info);
    if (result == VK_SUCCESS) {
        Logger::log("error", "--- VULKAN DEVICE FAULT DETECTED ---");
        Logger::log("error", "Description: %s", fault_info.description);

        for (uint32_t i = 0; i < fault_counts.addressInfoCount; i++) {
            Logger::log("error", ".pAddressInfos[%d]", i);
            Logger::log("error", "  addressType: %d",
                        fault_info.pAddressInfos[i].addressType);
            Logger::log("error", "  reportedAddress: %llu",
                        fault_info.pAddressInfos[i].reportedAddress);
            Logger::log("error", "  addressPrecision: %llu",
                        fault_info.pAddressInfos[i].addressPrecision);
        }
        for (uint32_t i = 0; i < fault_counts.vendorInfoCount; i++) {
            Logger::log("error", ".pVendorInfos[%d]", i);
            Logger::log("error", "  description: %s",
                        fault_info.pVendorInfos[i].description);
            Logger::log("error", "  vendorFaultCode: %llu",
                        fault_info.pVendorInfos[i].vendorFaultCode);
            Logger::log("error", "  vendorFaultData: %llu",
                        fault_info.pVendorInfos[i].vendorFaultData);
        }
        if (fault_info.pVendorBinaryData && fault_counts.vendorBinarySize > 0) {
            Logger::log("error",
                        "Vendor binary crash dump retrieved (%llu bytes).",
                        fault_counts.vendorBinarySize);
            std::string line_buffer;
            line_buffer.reserve(256);
            for (uint32_t i = 0; i < fault_counts.vendorBinarySize; i++) {
                char byte_str[4];
                snprintf(byte_str, sizeof(byte_str), "%02x ",
                         vendorBinaryData[i]);
                line_buffer += byte_str;
                if ((i + 1) % 64 == 0 ||
                    (i + 1) == fault_counts.vendorBinarySize) {
                    Logger::log("error", "  %s", line_buffer.c_str());
                    line_buffer.clear();
                }
            }
        }
        Logger::log("error", "--- END FAULT INFO ---");
    }
}

void ScopedTimestampQuery::Start() {
    if (m_startQueryId != UINT32_MAX) {
        m_cb->device->table.CmdWriteTimestamp(m_cb->handle, m_startStage,
                                              m_queryPool, m_startQueryId);
    }
}

void ScopedTimestampQuery::End() {
    if (m_startQueryId != UINT32_MAX) {
        m_cb->device->table.CmdWriteTimestamp(m_cb->handle, m_endStage,
                                              m_queryPool, m_startQueryId + 1);
    }
    m_startQueryId = UINT32_MAX;
}

std::pair<VkSemaphore, VkFence> StagingResources::MakeFence() {
    auto *dev = get_device(device);
    if (!dev || completed != VK_NULL_HANDLE)
        return {semaphore, completed};

    if (IsEmpty())
        return {semaphore, completed};

    auto [sem, fence] = dev->syncPool->Acquire();
    completed = fence;
    semaphore = sem;
    return {semaphore, completed};
}

void StagingResources::WaitForCompletion() {
    if (has_completed)
        return;
    auto *dev = get_device(device);
    if (!dev)
        return;
    VkResult result =
        dev->table.WaitForFences(device, 1, &completed, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        Logger::log("error", "WaitForFences failed with result %d", result);
        if (result == VK_ERROR_DEVICE_LOST) {
            LogDeviceFault(dev, "WaitForFences");
        }
    }
    has_completed = true;
}

ScopedTimestampQuery StagingResources::MakeScopedTimestampQuery(
    struct command_buffer *cb, const std::string &label, VkFormat format,
    uint64_t texture_size, VkPipelineStageFlagBits startStage,
    VkPipelineStageFlagBits endStage) {
    auto *dev = get_device(device);
    if (!dev || !dev->profile_transfers)
        return ScopedTimestampQuery{cb,           label,          format,
                                    texture_size, VK_NULL_HANDLE, UINT32_MAX,
                                    startStage,   endStage};

    if (queryPools.empty() ||
        queryPools.back().allocatedQueries + 2 > kPoolBlockSize) {
        // Allocate a new pool
        VkQueryPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = kPoolBlockSize,
            .pipelineStatistics = 0,
        };
        VkQueryPool newPool = VK_NULL_HANDLE;
        VkResult res =
            dev->table.CreateQueryPool(device, &poolInfo, nullptr, &newPool);
        if (res != VK_SUCCESS) {
            Logger::log("error", "Failed to allocate query pool block: %d",
                        res);
            return ScopedTimestampQuery{
                cb,         label,      format,  texture_size, VK_NULL_HANDLE,
                UINT32_MAX, startStage, endStage};
        }
        dev->table.CmdResetQueryPool(cb->handle, newPool, 0, kPoolBlockSize);
        queryPools.push_back({newPool, 0});
    }

    auto &activeBlock = queryPools.back();
    uint32_t startId = activeBlock.allocatedQueries;
    activeBlock.allocatedQueries += 2;

    auto pool = activeBlock.handle;
    size_t activePoolIdx = queryPools.size() - 1;

    trackedQueries.push_back(
        {label, format, texture_size, activePoolIdx, startId, startId + 1});
    return ScopedTimestampQuery{cb,   label,   format,     texture_size,
                                pool, startId, startStage, endStage};
}

void StagingResources::Cleanup() {
    if (freed)
        return;
    if (IsEmpty())
        return;
    freed = true;

    auto *dev = get_device(device);
    if (!dev)
        return;

    if (completed != VK_NULL_HANDLE && semaphore != VK_NULL_HANDLE) {
        {
            scoped_lock l(global_lock);
            dev->syncPool->Release(semaphore, completed);
        }
        completed = VK_NULL_HANDLE;
        semaphore = VK_NULL_HANDLE;
    }

    if (dev->profile_transfers) {
        auto now = std::chrono::system_clock::now();
        auto timestamp =
            std::chrono::time_point_cast<std::chrono::milliseconds>(now)
                .time_since_epoch()
                .count();
        Logger::log("info",
                    "Cleaning up batch %d with %d buffers, %d descriptors, and "
                    "%d tracked queries took %d ms",
                    id, stagingBuffers.size(), descriptorSets.size(),
                    trackedQueries.size(), timestamp - this->timestamp);
    }

    if (dev->profile_transfers && !queryPools.empty() &&
        !trackedQueries.empty()) {
        std::vector<std::vector<uint64_t>> allPoolResults(queryPools.size());
        bool success = true;

        for (size_t i = 0; i < queryPools.size(); ++i) {
            auto count = queryPools[i].allocatedQueries;
            allPoolResults[i].resize(count);
            VkResult result = dev->table.GetQueryPoolResults(
                device, queryPools[i].handle, 0, count,
                allPoolResults[i].size() * sizeof(uint64_t),
                allPoolResults[i].data(), sizeof(uint64_t),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            if (result != VK_SUCCESS) {
                Logger::log("error",
                            "GetQueryPoolResults failed for pool[%zu]: %d", i,
                            result);
                success = false;
                break;
            }
        }

        if (success) {
            float timestampPeriod =
                dev->props2.properties.limits.timestampPeriod;
            struct AggregatedStat {
                double totalTimeMs = 0.0;
                uint64_t totalSizeBytes = 0;
                uint32_t count = 0;
            };
            std::map<std::string, AggregatedStat> statsRollup;
            static std::map<std::string, AggregatedStat> globalStatsRollup;
            for (const auto &q : trackedQueries) {
                uint64_t startTicks =
                    allPoolResults[q.poolIndex][q.startQueryId];
                uint64_t endTicks = allPoolResults[q.poolIndex][q.endQueryId];

                if (endTicks >= startTicks) {
                    double durationMs = (double)(endTicks - startTicks) /
                                        (1000000.0f / timestampPeriod);

                    auto &stat = statsRollup[q.label];
                    stat.totalTimeMs += durationMs;
                    stat.totalSizeBytes += q.textureSize;
                    stat.count++;

                    auto &globalStat = globalStatsRollup[q.label];
                    globalStat.totalTimeMs += durationMs;
                    globalStat.totalSizeBytes += q.textureSize;
                    globalStat.count++;
                }
            }
            for (const auto &[label, stat] : statsRollup) {
                auto &globalStat = globalStatsRollup[label];
                double totalSizeMb = static_cast<double>(stat.totalSizeBytes) /
                                     (1024.0 * 1024.0);
                double globalTotalSizeMb =
                    static_cast<double>(globalStat.totalSizeBytes) /
                    (1024.0 * 1024.0);
                auto throughput = [](double sizeMb, double timeMs) -> double {
                    double timeSec = timeMs / 1000.0;
                    return (timeSec > 0.0) ? (sizeMb / timeSec) : 0.0;
                };

                Logger::log(
                    "info",
                    "  [%14s] Calls: %-5u | Time: %6.2f ms | Data: %6.1f MB | "
                    "Throughput: %6.1f MB/s (granularity: %.1fns)",
                    label.c_str(), stat.count, stat.totalTimeMs, totalSizeMb,
                    throughput(totalSizeMb, stat.totalTimeMs), timestampPeriod);

                Logger::log(
                    "info",
                    "  %22s: %-5u |  %11.2f ms |  %11.1f MB | Throughput: "
                    "%6.1f MB/s",
                    "+ total calls", globalStat.count, globalStat.totalTimeMs,
                    globalTotalSizeMb,
                    throughput(globalTotalSizeMb, globalStat.totalTimeMs),
                    timestampPeriod);
            }
        }
    }

    for (auto it = stagingBuffers.begin(); it != stagingBuffers.end();) {
        auto buf = std::move(*it);
        it = stagingBuffers.erase(it);
        if (!buf)
            continue;

        dev->table.DestroyBuffer(device, buf->handle, buf->alloc);
        dev->table.FreeMemory(device, buf->memory, buf->alloc);
    }

    for (auto imageView : stagingImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            dev->table.DestroyImageView(device, imageView, nullptr);
        }
    }
    stagingImageViews.clear();

    for (auto &descriptorSetBlock : descriptorSets) {
        dev->descriptorSetAllocator->free(descriptorSetBlock.first,
                                          descriptorSetBlock.second);
    }
    descriptorSets.clear();

    for (auto &poolBlock : queryPools) {
        if (poolBlock.handle != VK_NULL_HANDLE) {
            dev->table.DestroyQueryPool(device, poolBlock.handle, nullptr);
        }
    }
    queryPools.clear();
}

StagingResources::~StagingResources() { Cleanup(); }
