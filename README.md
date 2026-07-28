### Vulkan Profiles Potentially Missing Features/Extensions

#### Driver `54.3.0` (10 gpu profiles, 0 issues)
```text
No potential issues.
```

#### Driver `54.2.0` (10 gpu profiles, 0 issues)
```text
No potential issues.
```

#### Driver `54.1.0` (27 gpu profiles, 0 issues)
```text
No potential issues.
```

#### Driver `54.0.0` (10 gpu profiles, 0 issues)
```text
No potential issues.
```

#### Driver `53.0.0` (10 gpu profiles, 0 issues)
```text
No potential issues.
```

#### Driver `51.0.0` (16 gpu profiles, 1 issues)
```text
* [???] VK_EXT_external_memory_host (Required version: 1) - 16 gpu profiles (Mali-G57, Mali-G710, Mali-G715, Mali-G78)
```

#### Driver `50.0.0` (11 gpu profiles, 1 issues)
```text
* [???] VK_EXT_external_memory_host (Required version: 1) - 11 gpu profiles (Mali-G710, Mali-G715, Mali-G78)
```

#### Driver `49.1.0` (28 gpu profiles, 6 issues)
```text
* [???] VK_EXT_external_memory_host (Required version: 1) - 28 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G57, Mali-G68_MC4, Mali-G720_MC7, Mali-G720_MC8, Mali-G77_MC9, Mali-G78, Mali-G925-Immortalis_MC11, Mali-G925-Immortalis_MC12)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 5 gpu profiles (Mali-G52, Mali-G52_MC2)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 5 gpu profiles (Mali-G52, Mali-G52_MC2)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 5 gpu profiles (Mali-G52, Mali-G52_MC2)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 5 gpu profiles (Mali-G52, Mali-G52_MC2)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 5 gpu profiles (Mali-G52, Mali-G52_MC2)
```

#### Driver `48.0.0` (14 gpu profiles, 1 issues)
```text
* [???] VK_EXT_external_memory_host (Required version: 1) - 14 gpu profiles (Mali-G57_MC2, Mali-G710, Mali-G715, Mali-G78)
```

#### Driver `47.0.0` (11 gpu profiles, 1 issues)
```text
* [???] VK_EXT_external_memory_host (Required version: 1) - 11 gpu profiles (Mali-G710, Mali-G715, Mali-G78)
```

#### Driver `44.1.0` (12 gpu profiles, 1 issues)
```text
* [???] VK_EXT_external_memory_host (Required version: 1) - 12 gpu profiles (Mali-G310, Mali-G57, Mali-G615_MC2, Mali-G615_MC6, Mali-G720-Immortalis_MC12)
```

#### Driver `44.0.0` (10 gpu profiles, 3 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 10 gpu profiles (Mali-G310, Mali-G57, Mali-G710, Mali-G715, Mali-G78)
* [???] VK_EXT_external_memory_host (Required version: 1) - 10 gpu profiles (Mali-G310, Mali-G57, Mali-G710, Mali-G715, Mali-G78)
* [???] VK_ANDROID_external_memory_android_hardware_buffer (Required version: 1) - 1 gpu profiles (Mali-G310)
```

#### Driver `41.0.0` (5 gpu profiles, 2 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 5 gpu profiles (Mali-G57)
* [???] VK_EXT_external_memory_host (Required version: 1) - 5 gpu profiles (Mali-G57)
```

#### Driver `40.0.0` (7 gpu profiles, 5 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 7 gpu profiles (Mali-G57, Mali-G710, Mali-G78)
* [???] VK_EXT_external_memory_host (Required version: 1) - 7 gpu profiles (Mali-G57, Mali-G710, Mali-G78)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 7 gpu profiles (Mali-G57, Mali-G710, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features (missing feature struct) -> shaderBufferInt64Atomics - 7 gpu profiles (Mali-G57, Mali-G710, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features (missing feature struct) -> shaderSharedInt64Atomics - 7 gpu profiles (Mali-G57, Mali-G710, Mali-G78)
```

#### Driver `38.1.0` (50 gpu profiles, 12 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 50 gpu profiles (Mali-G52, Mali-G57_MC2, Mali-G610_MC4, Mali-G610_MC6, Mali-G68, Mali-G710, Mali-G710_MC10, Mali-G715-Immortalis_MC11, Mali-G72, Mali-G76, Mali-G77, Mali-G78)
* [???] VK_EXT_external_memory_host (Required version: 1) - 50 gpu profiles (Mali-G52, Mali-G57_MC2, Mali-G610_MC4, Mali-G610_MC6, Mali-G68, Mali-G710, Mali-G710_MC10, Mali-G715-Immortalis_MC11, Mali-G72, Mali-G76, Mali-G77, Mali-G78)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 50 gpu profiles (Mali-G52, Mali-G57_MC2, Mali-G610_MC4, Mali-G610_MC6, Mali-G68, Mali-G710, Mali-G710_MC10, Mali-G715-Immortalis_MC11, Mali-G72, Mali-G76, Mali-G77, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features (missing feature struct) -> shaderBufferInt64Atomics - 33 gpu profiles (Mali-G52, Mali-G57_MC2, Mali-G610_MC4, Mali-G68, Mali-G710, Mali-G715-Immortalis_MC11, Mali-G77, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features (missing feature struct) -> shaderSharedInt64Atomics - 33 gpu profiles (Mali-G52, Mali-G57_MC2, Mali-G610_MC4, Mali-G68, Mali-G710, Mali-G715-Immortalis_MC11, Mali-G77, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 17 gpu profiles (Mali-G610_MC6, Mali-G710_MC10, Mali-G72, Mali-G76, Mali-G77)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 17 gpu profiles (Mali-G610_MC6, Mali-G710_MC10, Mali-G72, Mali-G76, Mali-G77)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 15 gpu profiles (Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 15 gpu profiles (Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 15 gpu profiles (Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 15 gpu profiles (Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 15 gpu profiles (Mali-G52, Mali-G72, Mali-G76)
```

#### Driver `38.0.0` (5 gpu profiles, 5 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 5 gpu profiles (Mali-G78)
* [???] VK_EXT_external_memory_host (Required version: 1) - 5 gpu profiles (Mali-G78)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 5 gpu profiles (Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 5 gpu profiles (Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 5 gpu profiles (Mali-G78)
```

#### Driver `34.0.0` (22 gpu profiles, 10 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 22 gpu profiles (Mali-G52, Mali-G57, Mali-G76, Mali-G77, Mali-G78)
* [???] VK_EXT_external_memory_host (Required version: 1) - 22 gpu profiles (Mali-G52, Mali-G57, Mali-G76, Mali-G77, Mali-G78)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 22 gpu profiles (Mali-G52, Mali-G57, Mali-G76, Mali-G77, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 22 gpu profiles (Mali-G52, Mali-G57, Mali-G76, Mali-G77, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 22 gpu profiles (Mali-G52, Mali-G57, Mali-G76, Mali-G77, Mali-G78)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 10 gpu profiles (Mali-G52, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 10 gpu profiles (Mali-G52, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 10 gpu profiles (Mali-G52, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 10 gpu profiles (Mali-G52, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 10 gpu profiles (Mali-G52, Mali-G76)
```

#### Driver `32.1.0` (45 gpu profiles, 10 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 45 gpu profiles (Mali-G52_MC2, Mali-G57_MC2, Mali-G610_MC6, Mali-G68, Mali-G68_MC4, Mali-G710_MC10, Mali-G72, Mali-G76, Mali-G77_MC9, Mali-G78)
* [???] VK_EXT_external_memory_host (Required version: 1) - 45 gpu profiles (Mali-G52_MC2, Mali-G57_MC2, Mali-G610_MC6, Mali-G68, Mali-G68_MC4, Mali-G710_MC10, Mali-G72, Mali-G76, Mali-G77_MC9, Mali-G78)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 45 gpu profiles (Mali-G52_MC2, Mali-G57_MC2, Mali-G610_MC6, Mali-G68, Mali-G68_MC4, Mali-G710_MC10, Mali-G72, Mali-G76, Mali-G77_MC9, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 45 gpu profiles (Mali-G52_MC2, Mali-G57_MC2, Mali-G610_MC6, Mali-G68, Mali-G68_MC4, Mali-G710_MC10, Mali-G72, Mali-G76, Mali-G77_MC9, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 45 gpu profiles (Mali-G52_MC2, Mali-G57_MC2, Mali-G610_MC6, Mali-G68, Mali-G68_MC4, Mali-G710_MC10, Mali-G72, Mali-G76, Mali-G77_MC9, Mali-G78)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 15 gpu profiles (Mali-G52_MC2, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 15 gpu profiles (Mali-G52_MC2, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 15 gpu profiles (Mali-G52_MC2, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 15 gpu profiles (Mali-G52_MC2, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 15 gpu profiles (Mali-G52_MC2, Mali-G72, Mali-G76)
```

#### Driver `28.0.0` (12 gpu profiles, 19 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 12 gpu profiles (Mali-G57, Mali-T830, Mali-T880)
* [???] VK_EXT_external_memory_host (Required version: 1) - 12 gpu profiles (Mali-G57, Mali-T830, Mali-T880)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 12 gpu profiles (Mali-G57, Mali-T830, Mali-T880)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 12 gpu profiles (Mali-G57, Mali-T830, Mali-T880)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 12 gpu profiles (Mali-G57, Mali-T830, Mali-T880)
* [???] VK_ANDROID_external_memory_android_hardware_buffer (Required version: 1) - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VK_EXT_external_memory_dma_buf (Required version: 1) - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VK_KHR_8bit_storage (Required version: 1) - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VK_KHR_external_memory_fd (Required version: 1) - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VK_KHR_shader_float16_int8 (Required version: 1) - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VK_KHR_timeline_semaphore (Required version: 1) - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VkPhysicalDevice8BitStorageFeaturesKHR -> storageBuffer8BitAccess - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VkPhysicalDeviceShaderFloat16Int8FeaturesKHR -> shaderFloat16 - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VkPhysicalDeviceShaderFloat16Int8FeaturesKHR -> shaderInt8 - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 10 gpu profiles (Mali-T830, Mali-T880)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 10 gpu profiles (Mali-T830, Mali-T880)
```

#### Driver `27.0.0` (5 gpu profiles, 10 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 5 gpu profiles (Mali-G52)
* [???] VK_EXT_external_memory_host (Required version: 1) - 5 gpu profiles (Mali-G52)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 5 gpu profiles (Mali-G52)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 5 gpu profiles (Mali-G52)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 5 gpu profiles (Mali-G52)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 5 gpu profiles (Mali-G52)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 5 gpu profiles (Mali-G52)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 5 gpu profiles (Mali-G52)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 5 gpu profiles (Mali-G52)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 5 gpu profiles (Mali-G52)
```

#### Driver `26.0.0` (37 gpu profiles, 11 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 37 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G57_MC2, Mali-G610, Mali-G68_MC4, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4, Mali-G77, Mali-G77_MC9, Mali-G78)
* [???] VK_EXT_external_memory_host (Required version: 1) - 37 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G57_MC2, Mali-G610, Mali-G68_MC4, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4, Mali-G77, Mali-G77_MC9, Mali-G78)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 36 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G57_MC2, Mali-G68_MC4, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4, Mali-G77, Mali-G77_MC9, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 36 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G57_MC2, Mali-G68_MC4, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4, Mali-G77, Mali-G77_MC9, Mali-G78)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 36 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G57_MC2, Mali-G68_MC4, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4, Mali-G77, Mali-G77_MC9, Mali-G78)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 20 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 20 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 20 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 20 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 20 gpu profiles (Mali-G52, Mali-G52_MC2, Mali-G71, Mali-G72, Mali-G72_MP3, Mali-G76, Mali-G76_MC4)
* [???] VK_ANDROID_external_memory_android_hardware_buffer (Required version: 1) - 1 gpu profiles (Mali-G610)
```

#### Driver `21.0.0` (13 gpu profiles, 13 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 13 gpu profiles (Mali-G31, Mali-G52, Mali-G57, Mali-G57_MC3, Mali-G57_MC4, Mali-G77)
* [???] VK_EXT_external_memory_dma_buf (Required version: 1) - 13 gpu profiles (Mali-G31, Mali-G52, Mali-G57, Mali-G57_MC3, Mali-G57_MC4, Mali-G77)
* [???] VK_EXT_external_memory_host (Required version: 1) - 13 gpu profiles (Mali-G31, Mali-G52, Mali-G57, Mali-G57_MC3, Mali-G57_MC4, Mali-G77)
* [???] VK_KHR_external_memory_fd (Required version: 1) - 13 gpu profiles (Mali-G31, Mali-G52, Mali-G57, Mali-G57_MC3, Mali-G57_MC4, Mali-G77)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 13 gpu profiles (Mali-G31, Mali-G52, Mali-G57, Mali-G57_MC3, Mali-G57_MC4, Mali-G77)
* [???] VK_KHR_timeline_semaphore (Required version: 1) - 13 gpu profiles (Mali-G31, Mali-G52, Mali-G57, Mali-G57_MC3, Mali-G57_MC4, Mali-G77)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 13 gpu profiles (Mali-G31, Mali-G52, Mali-G57, Mali-G57_MC3, Mali-G57_MC4, Mali-G77)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 13 gpu profiles (Mali-G31, Mali-G52, Mali-G57, Mali-G57_MC3, Mali-G57_MC4, Mali-G77)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 5 gpu profiles (Mali-G31, Mali-G52)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 5 gpu profiles (Mali-G31, Mali-G52)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 5 gpu profiles (Mali-G31, Mali-G52)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 5 gpu profiles (Mali-G31, Mali-G52)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 5 gpu profiles (Mali-G31, Mali-G52)
```

#### Driver `20.0.0` (19 gpu profiles, 13 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 19 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4, Mali-G77)
* [???] VK_EXT_external_memory_dma_buf (Required version: 1) - 19 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4, Mali-G77)
* [???] VK_EXT_external_memory_host (Required version: 1) - 19 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4, Mali-G77)
* [???] VK_KHR_external_memory_fd (Required version: 1) - 19 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4, Mali-G77)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 19 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4, Mali-G77)
* [???] VK_KHR_timeline_semaphore (Required version: 1) - 19 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4, Mali-G77)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 19 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4, Mali-G77)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 19 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4, Mali-G77)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 16 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 16 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 16 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 16 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 16 gpu profiles (Mali-G52_MC2, Mali-G71_MP2, Mali-G72_MP3, Mali-G76_MC4)
```

#### Driver `19.0.0` (17 gpu profiles, 13 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VK_EXT_external_memory_dma_buf (Required version: 1) - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VK_EXT_external_memory_host (Required version: 1) - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VK_KHR_external_memory_fd (Required version: 1) - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VK_KHR_timeline_semaphore (Required version: 1) - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 17 gpu profiles (Mali-G52, Mali-G71, Mali-G72, Mali-G76)
```

#### Driver `18.0.0` (23 gpu profiles, 13 issues)
```text
* [???] VK_EXT_device_fault (Required version: 1) - 23 gpu profiles (Mali-G51, Mali-G52, Mali-G610, Mali-G72, Mali-G76)
* [???] VK_EXT_external_memory_host (Required version: 1) - 23 gpu profiles (Mali-G51, Mali-G52, Mali-G610, Mali-G72, Mali-G76)
* [???] VK_EXT_external_memory_dma_buf (Required version: 1) - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VK_KHR_external_memory_fd (Required version: 1) - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VK_KHR_timeline_semaphore (Required version: 1) - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 18 gpu profiles (Mali-G51, Mali-G52, Mali-G72, Mali-G76)
```

#### Driver `16.0.0` (5 gpu profiles, 19 issues)
```text
* [???] VK_ANDROID_external_memory_android_hardware_buffer (Required version: 1) - 5 gpu profiles (Mali-G71)
* [???] VK_EXT_device_fault (Required version: 1) - 5 gpu profiles (Mali-G71)
* [???] VK_EXT_external_memory_dma_buf (Required version: 1) - 5 gpu profiles (Mali-G71)
* [???] VK_EXT_external_memory_host (Required version: 1) - 5 gpu profiles (Mali-G71)
* [???] VK_KHR_8bit_storage (Required version: 1) - 5 gpu profiles (Mali-G71)
* [???] VK_KHR_external_memory_fd (Required version: 1) - 5 gpu profiles (Mali-G71)
* [???] VK_KHR_shader_atomic_int64 (Required version: 1) - 5 gpu profiles (Mali-G71)
* [???] VK_KHR_shader_float16_int8 (Required version: 1) - 5 gpu profiles (Mali-G71)
* [???] VK_KHR_timeline_semaphore (Required version: 1) - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDevice8BitStorageFeaturesKHR -> storageBuffer8BitAccess - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderBufferInt64Atomics - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDeviceShaderAtomicInt64Features -> shaderSharedInt64Atomics - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDeviceShaderFloat16Int8FeaturesKHR -> shaderFloat16 - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDeviceShaderFloat16Int8FeaturesKHR -> shaderInt8 - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorIndexing - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingSampledImageUpdateAfterBind - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingUpdateUnusedWhilePending - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDeviceVulkan12Features -> descriptorBindingPartiallyBound - 5 gpu profiles (Mali-G71)
* [???] VkPhysicalDeviceVulkan12Features -> runtimeDescriptorArray - 5 gpu profiles (Mali-G71)
```
