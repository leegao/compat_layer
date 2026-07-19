#ifndef LAYER_HPP
#define LAYER_HPP

#include "null_descriptors.hpp"
#include "staging_resources.hpp"
#include "vulkan/vk_layer.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.h>

#undef VK_LAYER_EXPORT
#if defined(WIN32)
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)
#else
#define VK_LAYER_EXPORT extern "C"
#endif

#define VK_DRIVER_ID_QUALCOMM_PROPRIETARY 8
#define VK_DRIVER_ID_ARM_PROPRIETARY 9
#define VK_DRIVER_ID_MESA_TURNIP 18
#define VK_DRIVER_ID_SAMSUNG_PROPRIETARY 21

extern std::unordered_map<void *, VkLayerInstanceDispatchTable>
    instanceDispatch;
extern std::unordered_map<void *, VkInstance> instanceMap;
extern std::unordered_map<void *, std::unordered_set<std::string>>
    deviceExtensionsMap;
template <typename T> void *GetKey(T item) { return *(void **)item; }
void *GetInstanceKey(VkPhysicalDevice physicalDevice);

extern std::mutex global_lock;
typedef std::lock_guard<std::mutex> scoped_lock;

class SyncPool {
  public:
    explicit SyncPool(VkDevice device) : device(device) {}
    ~SyncPool();

    std::pair<VkSemaphore, VkFence> Acquire();

    void Release(VkSemaphore sem, VkFence fence) {
        freeSemaphores.push_back(sem);
        freeFences.push_back(fence);
    }

  private:
    VkDevice device;
    std::vector<VkFence> freeFences;
    std::vector<VkSemaphore> freeSemaphores;
};

class DescriptorSetAllocator {
  public:
    struct PoolSizes {
        std::vector<VkDescriptorPoolSize> sizes;
        uint32_t maxSets = 1024;
    };

    explicit DescriptorSetAllocator(struct device *device,
                                    const PoolSizes &defaultSizes)
        : device(device), poolSizes(defaultSizes) {
        createNewPool(&activePool);
    }
    ~DescriptorSetAllocator() { cleanup(); }

    void cleanup();
    VkResult allocate(VkDescriptorSetLayout layout, VkDescriptorPool *pool,
                      VkDescriptorSet *descriptors);
    void free(VkDescriptorPool pool, VkDescriptorSet descriptors);
    uint64_t allocatedCount() const { return allocated_count; }

  private:
    VkResult createNewPool(VkDescriptorPool *descriptor_pool);

    struct device *device = nullptr;
    VkDescriptorPool activePool = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> exhaustedPools;
    std::unordered_map<VkDescriptorPool, uint32_t> occupancy;
    PoolSizes poolSizes;
    std::mutex lock;
    uint64_t allocated_count = 0;
    const size_t maxEmptyPoolsToReserve = 2;
};

struct device {
    VkDevice handle;
    uint32_t deviceId;
    VkPhysicalDevice physical;
    VkPhysicalDeviceProperties2 props2;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceDriverProperties driverProps;
    VkPhysicalDeviceMemoryProperties memoryProps;
    VkLayerDispatchTable table;
    VkQueue queue;
    uint32_t queueFamilyIndex;
    uint32_t memoryIndex;
    int profile_transfers = 0;
    int sample_gpu_counters = 0;
    int emulate_push_descriptors = 0;
    int emulate_null_descriptor = 0;
    int emulate_precise_null_descriptor = 0;
    int emulate_sparse_binding = 0;
    const VkAllocationCallbacks *alloc;
    std::unique_ptr<SyncPool> syncPool;
    std::unique_ptr<DescriptorSetAllocator> descriptorSetAllocator;
    std::vector<std::unique_ptr<StagingResources>> stagingResourcesQueue;
    std::condition_variable hasCleanupWork;
    std::thread finalizer_thread;
    std::atomic_bool stop_thread{false};
    std::string dump_buffers_path;
    bool has_more_layers = false;
    null_descriptor_emulation null_descriptors;
    std::atomic<VkDeviceSize> sparseCommittedBytes{0};
};

struct device *get_device(VkDevice);

#endif // LAYER_HPP
