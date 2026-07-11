# compat_layer

---

Set `ENABLE_DXVK_MALI_COMPAT_LAYER=1` / `VK_INSTANCE_LAYERS=VK_LAYER_COMPAT_DxvkMaliCompatLayer`

Difference between Mali v1.r44p1 and v1.r54p3:

```
* VK_EXT_depth_clip_enable (Required version: 1)
* VK_EXT_robustness2 (Required version: 1)
* VK_KHR_push_descriptor (Required version: 1)
* VkPhysicalDeviceDepthClipEnableFeaturesEXT (missing feature struct) -> depthClipEnable
* VkPhysicalDeviceFeatures -> dualSrcBlend
* VkPhysicalDeviceFeatures -> logicOp
* VkPhysicalDeviceRobustness2FeaturesEXT (missing feature struct):
    * robustBufferAccess2
    * robustImageAccess2
    * nullDescriptor
* VkPhysicalDevicePushDescriptorPropertiesKHR -> maxPushDescriptors: Not specified/supported by device (Requires 32)
* VkPhysicalDeviceVulkan12Properties:
    * ~maxPerStageDescriptorUpdateAfterBindStorageBuffers: Device value 500000 is less than required minimum of 1000000~
    * ~maxPerStageDescriptorUpdateAfterBindSampledImages: Device value 500000 is less than required minimum of 1000000~
    * ~maxPerStageDescriptorUpdateAfterBindStorageImages: Device value 500000 is less than required minimum of 1000000~
```

---

```
Driver: v1.r44p1-01eac0.148b774f93332d7e562320793fb6c4c9
Vulkan API: 1.3.247

[FAILED] Profile: VP_D3D12_FL_12_0_baseline
  Description: Minimum baseline to create a device with FL 12.0.
  Target Vulkan API: 1.3.204
  - Missing/Insufficient Extensions:
      * VK_EXT_depth_clip_enable (Required version: 1)
      * VK_EXT_robustness2 (Required version: 1)
      * VK_EXT_vertex_attribute_divisor (Required version: 3)
      * VK_KHR_push_descriptor (Required version: 1)
  - Missing Required Feature Flags:
      * VkPhysicalDeviceDepthClipEnableFeaturesEXT (missing feature struct) -> depthClipEnable
      * VkPhysicalDeviceFeatures -> dualSrcBlend
      * VkPhysicalDeviceFeatures -> fillModeNonSolid
      * VkPhysicalDeviceFeatures -> multiViewport
      * VkPhysicalDeviceFeatures -> textureCompressionBC
      * VkPhysicalDeviceFeatures -> pipelineStatisticsQuery
      * VkPhysicalDeviceFeatures -> shaderClipDistance
      * VkPhysicalDeviceFeatures -> shaderCullDistance
      * VkPhysicalDeviceFeatures -> vertexPipelineStoresAndAtomics
      * VkPhysicalDeviceFeatures -> logicOp
      * VkPhysicalDeviceFeatures -> sparseBinding
      * VkPhysicalDeviceFeatures -> sparseResidencyAliased
      * VkPhysicalDeviceFeatures -> sparseResidencyBuffer
      * VkPhysicalDeviceFeatures -> sparseResidencyImage2D
      * VkPhysicalDeviceFeatures -> shaderResourceResidency
      * VkPhysicalDeviceFeatures -> shaderResourceMinLod
      * VkPhysicalDeviceRobustness2FeaturesEXT (missing feature struct) -> robustBufferAccess2
      * VkPhysicalDeviceRobustness2FeaturesEXT (missing feature struct) -> robustImageAccess2
      * VkPhysicalDeviceRobustness2FeaturesEXT (missing feature struct) -> nullDescriptor
      * VkPhysicalDeviceTransformFeedbackFeaturesEXT -> geometryStreams
      * VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT (missing feature struct) -> vertexAttributeInstanceRateDivisor
      * VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT (missing feature struct) -> vertexAttributeInstanceRateZeroDivisor
  - Failed Properties or Limits Constraints:
      * VkPhysicalDeviceProperties -> sparseProperties -> residencyStandard2DBlockShape: Device value is False (Expected True)
      * VkPhysicalDeviceProperties -> sparseProperties -> residencyNonResidentStrict: Device value is False (Expected True)
      * VkPhysicalDevicePushDescriptorPropertiesKHR -> maxPushDescriptors: Not specified/supported by device (Requires 32)
      * VkPhysicalDeviceVulkan12Properties -> maxPerStageDescriptorUpdateAfterBindStorageBuffers: Device value 500000 is less than required minimum of 1000000
      * VkPhysicalDeviceVulkan12Properties -> maxPerStageDescriptorUpdateAfterBindSampledImages: Device value 500000 is less than required minimum of 1000000
      * VkPhysicalDeviceVulkan12Properties -> maxPerStageDescriptorUpdateAfterBindStorageImages: Device value 500000 is less than required minimum of 1000000
      * VkPhysicalDeviceVulkan13Properties -> storageTexelBufferOffsetSingleTexelAlignment: Device value is False (Expected True)
  - Failed Queue Family Constraints:
      * Requires queue family with flags: ['VK_QUEUE_SPARSE_BINDING_BIT'] (Minimum Count: 1)

```

vs

```
Driver: v1.r54p3-00eac0.d6609729a0c2466c24fd0530143c8edf
Vulkan API: 1.4.343

[FAILED] Profile: VP_D3D12_FL_12_0_baseline
  Description: Minimum baseline to create a device with FL 12.0.
  Target Vulkan API: 1.3.204
  - Missing/Insufficient Extensions:
      * VK_EXT_vertex_attribute_divisor (Required version: 3)
  - Missing Required Feature Flags:
      * VkPhysicalDeviceFeatures -> fillModeNonSolid
      * VkPhysicalDeviceFeatures -> multiViewport
      * VkPhysicalDeviceFeatures -> textureCompressionBC
      * VkPhysicalDeviceFeatures -> pipelineStatisticsQuery
      * VkPhysicalDeviceFeatures -> shaderClipDistance
      * VkPhysicalDeviceFeatures -> shaderCullDistance
      * VkPhysicalDeviceFeatures -> vertexPipelineStoresAndAtomics
      * VkPhysicalDeviceFeatures -> sparseBinding
      * VkPhysicalDeviceFeatures -> sparseResidencyAliased
      * VkPhysicalDeviceFeatures -> sparseResidencyBuffer
      * VkPhysicalDeviceFeatures -> sparseResidencyImage2D
      * VkPhysicalDeviceFeatures -> shaderResourceResidency
      * VkPhysicalDeviceFeatures -> shaderResourceMinLod
      * VkPhysicalDeviceRobustness2FeaturesEXT -> robustBufferAccess2
      * VkPhysicalDeviceRobustness2FeaturesEXT -> robustImageAccess2
      * VkPhysicalDeviceTransformFeedbackFeaturesEXT -> geometryStreams
      * VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT (missing feature struct) -> vertexAttributeInstanceRateDivisor
      * VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT (missing feature struct) -> vertexAttributeInstanceRateZeroDivisor
  - Failed Properties or Limits Constraints:
      * VkPhysicalDeviceProperties -> sparseProperties -> residencyStandard2DBlockShape: Device value is False (Expected True)
      * VkPhysicalDeviceProperties -> sparseProperties -> residencyNonResidentStrict: Device value is False (Expected True)
      * VkPhysicalDeviceVulkan13Properties -> storageTexelBufferOffsetSingleTexelAlignment: Device value is False (Expected True)
  - Failed Queue Family Constraints:
      * Requires queue family with flags: ['VK_QUEUE_SPARSE_BINDING_BIT'] (Minimum Count: 1)
```

---

Just instance spoofing:

https://www.diffchecker.com/4jeOMPAO/

v44 vs v54

```
[info]: Spoofing core feature logicOp: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing extension VK_EXT_depth_clip_enable: version 1
[info]: Spoofing extension VK_KHR_push_descriptor: version 1
[info]: Spoofing feature vertexAttributeInstanceRateDivisor in VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature nullDescriptor in VkPhysicalDeviceRobustness2FeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature depthClipEnable in VkPhysicalDeviceDepthClipEnableFeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing property maxPushDescriptors in VkPhysicalDevicePushDescriptorPropertiesKHR: reporting = 32, driver = 0
...
[info]: Masking extension: VK_KHR_push_descriptor
[info]: Masking extension: VK_EXT_depth_clip_enable
[info]: Masking extension: VK_EXT_robustness2
[info]: Masking feature dualSrcBlend in VkPhysicalDeviceFeatures
[info]: Masking feature logicOp in VkPhysicalDeviceFeatures
[info]: Masking feature vertexAttributeInstanceRateDivisor in VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT
[info]: Masking feature nullDescriptor in VkPhysicalDeviceRobustness2FeaturesEXT
...
[info]: Dispatch not found: vkCmdBindDescriptorSets2 / vkCmdBindDescriptorSets2KHR
[info]: Dispatch not found: vkCmdPushConstants2 / vkCmdPushConstants2KHR
...
3546.313:00e8:00ec:info:vkd3d-proton:vkd3d_bindless_state_get_bindless_flags: Device does not support VK_EXT_mutable_descriptor_type (or VALVE).
--- 698084.129:00e8:00ec:info:vkd3d-proton:vkd3d_bindless_state_add_binding: Device supports VK_EXT_descriptor_buffer! (not on the moto)

```

---

## D3D12 Cube

Works even without nullDescriptors emulation

Moto g86 (r44): working

```
info:  Game: AIO-Graphics-Test.exe
info:  DXVK: v2.3.1
info:  Vulkan: Found vkGetInstanceProcAddr in winevulkan.dll @ 0x4ffd195bd0
info:  Built-in extension providers:
info:    Win32 WSI
info:    OpenVR
info:    OpenXR
info:  OpenVR: could not open registry key, status 2
info:  OpenVR: Failed to locate module
info:  Enabled instance extensions:
info:    VK_EXT_surface_maintenance1
info:    VK_KHR_get_surface_capabilities2
info:    VK_KHR_surface
info:    VK_KHR_win32_surface
[info]: Spoofing core property sparseProperties.residencyNonResidentStrict: reporting = 1, driver = 0
[info]: Spoofing core property sparseProperties.residencyStandard2DBlockShape: reporting = 1, driver = 0
[info]: Spoofing extension VK_EXT_depth_clip_enable: version 1
[info]: Spoofing extension VK_EXT_vertex_attribute_divisor: version 3
[info]: Spoofing extension VK_KHR_push_descriptor: version 1
[info]: Spoofing property maxCustomBorderColorSamplers in VkPhysicalDeviceCustomBorderColorPropertiesEXT: reporting = 2048, driver = 4294967295
[info]: Spoofing property storageTexelBufferOffsetSingleTexelAlignment in VkPhysicalDeviceVulkan13Properties: reporting = 1, driver = 0
[info]: Spoofing property maxPerStageDescriptorUpdateAfterBindSampledImages in VkPhysicalDeviceVulkan12Properties: reporting = 1000000, driver = 500000
[info]: Spoofing property maxPerStageDescriptorUpdateAfterBindStorageBuffers in VkPhysicalDeviceVulkan12Properties: reporting = 1000000, driver = 500000
[info]: Spoofing property maxPerStageDescriptorUpdateAfterBindStorageImages in VkPhysicalDeviceVulkan12Properties: reporting = 1000000, driver = 500000
[info]: Spoofing property subgroupSupportedOperations in VkPhysicalDeviceVulkan11Properties: reporting = 159, driver = 0
[info]: Spoofing property subgroupSupportedStages in VkPhysicalDeviceVulkan11Properties: reporting = 48, driver = 0
[info]: Spoofing core feature logicOp: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature pipelineStatisticsQuery: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature shaderResourceMinLod: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature shaderResourceResidency: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature sparseBinding: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature sparseResidencyAliased: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature sparseResidencyBuffer: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature sparseResidencyImage2D: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature vertexPipelineStoresAndAtomics: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature vertexAttributeInstanceRateDivisor in VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature vertexAttributeInstanceRateZeroDivisor in VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature geometryStreams in VkPhysicalDeviceTransformFeedbackFeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature nullDescriptor in VkPhysicalDeviceRobustness2FeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature robustBufferAccess2 in VkPhysicalDeviceRobustness2FeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature robustImageAccess2 in VkPhysicalDeviceRobustness2FeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature depthClipEnable in VkPhysicalDeviceDepthClipEnableFeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
info:  Wrapper(Mali-G615 MC2):
info:    Driver : Wrapper driver 44.1.0
info:    Memory Heap[0]:
info:      Size: 7353 MiB
info:      Flags: 0x1
info:      Memory Type[0]: Property Flags = 0x7
info:      Memory Type[1]: Property Flags = 0xb
info:      Memory Type[2]: Property Flags = 0x11
info:    Memory Heap[1]:
info:      Size: 100 MiB
info:      Flags: 0x1
info:      Memory Type[3]: Property Flags = 0x21
warn:  DXGI: Found monitors not associated with any adapter, using fallback
info:  Adapter LUID 0: 0:3f4
4134.543:00e4:00e8:info:vkd3d-proton:vkd3d_instance_apply_application_workarounds: Program name: "AIO-Graphics-Test.exe" (hash: a88877c11331fe8d)
4134.546:00e4:00e8:info:vkd3d-proton:vkd3d_instance_deduce_config_flags_from_environment: shader_cache is used, global_pipeline_cache is enforced.
4134.546:00e4:00e8:info:vkd3d-proton:vkd3d_config_flags_init_once: VKD3D_CONFIG=''.
4134.551:00e4:00e8:info:vkd3d-proton:vkd3d_get_vk_version: vkd3d-proton - applicationVersion: 2.14.1.
4134.551:00e4:00e8:info:vkd3d-proton:vkd3d_instance_init: vkd3d-proton - build: 0d66699b1b1e250.
[info]: Spoofing property maxPushDescriptors in VkPhysicalDevicePushDescriptorPropertiesKHR: reporting = 32, driver = 0
4134.612:00e4:00e8:info:vkd3d-proton:vkd3d_init_device_caps: Not all relevant pipeline stages are supported by EXT_dgc. Skipping EXT.
[info]: Masking extension: VK_KHR_push_descriptor
[info]: Masking extension: VK_EXT_depth_clip_enable
[info]: Masking extension: VK_EXT_robustness2
[info]: Masking extension: VK_EXT_vertex_attribute_divisor
[info]: Adding extension VK_EXT_device_fault
[info]: Enabling VK_EXT_device_fault features
[info]: Masking feature dualSrcBlend in VkPhysicalDeviceFeatures
[info]: Masking feature logicOp in VkPhysicalDeviceFeatures
[info]: Masking feature pipelineStatisticsQuery in VkPhysicalDeviceFeatures
[info]: Masking feature shaderResourceMinLod in VkPhysicalDeviceFeatures
[info]: Masking feature shaderResourceResidency in VkPhysicalDeviceFeatures
[info]: Masking feature sparseBinding in VkPhysicalDeviceFeatures
[info]: Masking feature sparseResidencyAliased in VkPhysicalDeviceFeatures
[info]: Masking feature sparseResidencyBuffer in VkPhysicalDeviceFeatures
[info]: Masking feature sparseResidencyImage2D in VkPhysicalDeviceFeatures
[info]: Masking feature vertexPipelineStoresAndAtomics in VkPhysicalDeviceFeatures
[info]: Masking feature vertexAttributeInstanceRateDivisor in VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT
[info]: Masking feature vertexAttributeInstanceRateZeroDivisor in VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT
[info]: Masking feature geometryStreams in VkPhysicalDeviceTransformFeedbackFeaturesEXT
[info]: Masking feature nullDescriptor in VkPhysicalDeviceRobustness2FeaturesEXT
[info]: Masking feature robustBufferAccess2 in VkPhysicalDeviceRobustness2FeaturesEXT
[info]: Masking feature robustImageAccess2 in VkPhysicalDeviceRobustness2FeaturesEXT
[info]: Masking feature depthClipEnable in VkPhysicalDeviceDepthClipEnableFeaturesEXT
[info]: Dispatch not found: vkCmdBindDescriptorSets2 / vkCmdBindDescriptorSets2KHR
[info]: Dispatch not found: vkCmdPushConstants2 / vkCmdPushConstants2KHR
[info]: Dispatch not found: vkCmdDrawIndirectCountAMD / vkCmdDrawIndirectCountAMDKHR
[info]: Dispatch not found: vkCmdDrawIndexedIndirectCountAMD / vkCmdDrawIndexedIndirectCountAMDKHR
[info]: Dispatch not found: vkCreateSharedSwapchainsKHR / vkCreateSharedSwapchainsKHRKHR
[info]: Dispatch not found: vkDebugMarkerSetObjectTagEXT / vkDebugMarkerSetObjectTagEXTKHR
[info]: Dispatch not found: vkDebugMarkerSetObjectNameEXT / vkDebugMarkerSetObjectNameEXTKHR
[info]: Dispatch not found: vkCmdDebugMarkerBeginEXT / vkCmdDebugMarkerBeginEXTKHR
[info]: Dispatch not found: vkCmdDebugMarkerEndEXT / vkCmdDebugMarkerEndEXTKHR
[info]: Dispatch not found: vkCmdDebugMarkerInsertEXT / vkCmdDebugMarkerInsertEXTKHR
[info]: Dispatch not found: vkDeviceSetApiDumpState / vkDeviceSetApiDumpStateKHR
4134.659:00e4:00e8:info:vkd3d-proton:vkd3d_memory_info_decide_hvv_usage: Topology: UMA-like topology.
4134.660:00e4:00e8:info:vkd3d-proton:vkd3d_memory_info_upload_hvv_memory_properties: Topology: HVV usage is allowed, using DEVICE_LOCAL | HOST_COHERENT for UPLOAD.
4134.662:00e4:00e8:info:vkd3d-proton:vkd3d_bindless_state_get_bindless_flags: Device does not support VK_EXT_mutable_descriptor_type (or VALVE).
[info]: Binding 0 exceeds 100000 descriptors: descriptorType=0, descriptorCount=500000
[info]: Binding 2 exceeds 100000 descriptors: descriptorType=7, descriptorCount=1000000
[info]: Binding 2 exceeds 100000 descriptors: descriptorType=7, descriptorCount=500000
[info]: Binding 0 exceeds 100000 descriptors: descriptorType=4, descriptorCount=1000000
[info]: Binding 0 exceeds 100000 descriptors: descriptorType=4, descriptorCount=500000
[info]: Binding 0 exceeds 100000 descriptors: descriptorType=2, descriptorCount=1000000
[info]: Binding 0 exceeds 100000 descriptors: descriptorType=2, descriptorCount=500000
[info]: Binding 0 exceeds 100000 descriptors: descriptorType=5, descriptorCount=1000000
[info]: Binding 0 exceeds 100000 descriptors: descriptorType=5, descriptorCount=500000
[info]: Binding 0 exceeds 100000 descriptors: descriptorType=3, descriptorCount=1000000
[info]: Binding 0 exceeds 100000 descriptors: descriptorType=3, descriptorCount=500000
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e310
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e310
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e310
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e680
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e520
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e520
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e680
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e680
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e4e0
[info]: vkCreateDescriptorSetLayout: VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR device=0xb400007d5a69ef00 createInfo=0x21e4e0
[info]: Calling vkCreateGraphicsPipelines
[info]: vkCreateGraphicsPipelines called: pStages[0]= 0x1c000000001c
[info]: vkCreateGraphicsPipelines called: pStages[1]= 0x7e000000007e
[info]: Calling vkCreateGraphicsPipelines
[info]: vkCreateGraphicsPipelines called: pStages[0]= 0x1c000000001c
[info]: vkCreateGraphicsPipelines called: pStages[1]= 0x800000000080
4134.890:00e4:00e8:fixme:vkd3d-proton:d3d12_device_caps_init_feature_options1: TotalLaneCount = 512, may be inaccurate.
4134.891:00e4:00e8:info:vkd3d-proton:vkd3d_pipeline_library_init_disk_cache: Remapping VKD3D_SHADER_CACHE to: vkd3d-proton.cache.
4134.892:00e4:00e8:info:vkd3d-proton:vkd3d_pipeline_library_init_disk_cache: Attempting to load disk cache from: vkd3d-proton.cache.
warn:  CreateDXGIFactory2: Ignoring flags
info:  Wrapper(Mali-G615 MC2):
info:    Driver : Wrapper driver 44.1.0
info:    Memory Heap[0]:
info:      Size: 7353 MiB
info:      Flags: 0x1
info:      Memory Type[0]: Property Flags = 0x7
info:      Memory Type[1]: Property Flags = 0xb
info:      Memory Type[2]: Property Flags = 0x11
info:    Memory Heap[1]:
info:      Size: 100 MiB
info:      Flags: 0x1
4134.898:00e4:00f0:info:vkd3d-proton:vkd3d_pipeline_library_disk_thread_main: Performing async setup of stream archive ...
info:      Memory Type[3]: Property Flags = 0x21
warn:  DXGI: Found monitors not associated with any adapter, using fallback
4134.904:00e4:00e8:info:vkd3d-proton:dxgi_vk_swap_chain_init: Creating swapchain (632 x 454), BufferCount = 2.
4134.912:00e4:00e8:info:vkd3d-proton:dxgi_vk_swap_chain_init_sync_objects: Ensure maximum latency of 3 frames with KHR_present_wait.
4134.918:00e4:00e8:info:vkd3d-proton:dxgi_vk_swap_chain_init_waiter_thread: Enabling present wait path for frame latency.
4134.921:00e4:00f0:info:vkd3d-proton:vkd3d_pipeline_library_disk_cache_merge: No write cache exists. No need to merge any disk caches.
4134.919:00e4:00e8:info:vkd3d-proton:dxgi_vk_swap_chain_init_sleep_state: Timer interval is 1.0 ms.
4134.943:00e4:00f0:info:vkd3d-proton:vkd3d_pipeline_library_disk_cache_initial_setup: Merging pipeline libraries took 41.019 ms.
4134.947:00e4:00f0:info:vkd3d-proton:vkd3d_pipeline_library_disk_cache_initial_setup: Mapping read-only cache took 3.147 ms.
4134.951:00e4:00f0:info:vkd3d-proton:vkd3d_pipeline_library_disk_cache_initial_setup: Parsing stream archive took 3.520 ms.
4134.952:00e4:00f0:info:vkd3d-proton:vkd3d_pipeline_library_disk_thread_main: Done performing async setup of stream archive.
[info]: Calling vkCreateGraphicsPipelines
[info]: vkCreateGraphicsPipelines called: pStages[0]= 0x9a000000009a
[info]: vkCreateGraphicsPipelines called: pStages[1]= 0x9b000000009b
err:   readMonitorEdidFromKey: Failed to get EDID reg key size
err:   DXGI: Failed to parse display metadata + colorimetry info, using blank.
4135.302:00e4:00f8:info:vkd3d-proton:dxgi_vk_swap_chain_recreate_swapchain_in_present_task: Got 3 swapchain images.
[info]: Calling vkCreateGraphicsPipelines
[info]: vkCreateGraphicsPipelines called: pStages[0]= 0x510000000051
[info]: vkCreateGraphicsPipelines called: pStages[1]= 0x520000000052
4135.330:00e4:00f8:info:vkd3d-proton:dxgi_vk_swap_chain_recreate_swapchain_in_present_task: Got 3 swapchain images.
```

---

Pixel 8 Pro (r54): rendered fine

```
[info]: Spoofing core feature pipelineStatisticsQuery: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature shaderResourceMinLod: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature shaderResourceResidency: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature sparseBinding: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature sparseResidencyAliased: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature sparseResidencyBuffer: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature sparseResidencyImage2D: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core feature vertexPipelineStoresAndAtomics: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing core property sparseProperties.residencyNonResidentStrict: reporting = 1, driver = 0
[info]: Spoofing core property sparseProperties.residencyStandard2DBlockShape: reporting = 1, driver = 0
[info]: Spoofing extension VK_EXT_vertex_attribute_divisor: version 3
[info]: Spoofing property maxCustomBorderColorSamplers in VkPhysicalDeviceCustomBorderColorPropertiesEXT: reporting = 2048, driver = 4294967295
[info]: Spoofing property storageTexelBufferOffsetSingleTexelAlignment in VkPhysicalDeviceVulkan13Properties: reporting = 1, driver = 0
[info]: Spoofing property maxPerStageDescriptorUpdateAfterBindSampledImages in VkPhysicalDeviceVulkan12Properties: reporting = 1000000, driver = 8388608
[info]: Spoofing property maxPerStageDescriptorUpdateAfterBindStorageBuffers in VkPhysicalDeviceVulkan12Properties: reporting = 1000000, driver = 8388608
[info]: Spoofing property maxPerStageDescriptorUpdateAfterBindStorageImages in VkPhysicalDeviceVulkan12Properties: reporting = 1000000, driver = 8388608
[info]: Spoofing property subgroupSupportedOperations in VkPhysicalDeviceVulkan11Properties: reporting = 159, driver = 0
[info]: Spoofing property subgroupSupportedStages in VkPhysicalDeviceVulkan11Properties: reporting = 48, driver = 0
[info]: Spoofing feature vertexAttributeInstanceRateZeroDivisor in VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature geometryStreams in VkPhysicalDeviceTransformFeedbackFeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature robustBufferAccess2 in VkPhysicalDeviceRobustness2FeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
[info]: Spoofing feature robustImageAccess2 in VkPhysicalDeviceRobustness2FeaturesEXT: reporting = VK_TRUE, driver = VK_FALSE
info:  Wrapper(Mali-G715):
info:    Driver : Wrapper driver 54.2.0
info:    Memory Heap[0]:
info:      Size: 11157 MiB
info:      Flags: 0x1
info:      Memory Type[0]: Property Flags = 0x7
info:      Memory Type[1]: Property Flags = 0xb
info:      Memory Type[2]: Property Flags = 0x11
info:    Memory Heap[1]:
info:      Size: 100 MiB
info:      Flags: 0x1
info:      Memory Type[3]: Property Flags = 0x21
warn:  DXGI: Found monitors not associated with any adapter, using fallback
info:  Adapter LUID 0: 0:3f4
698083.263:00e8:00ec:info:vkd3d-proton:vkd3d_instance_apply_application_workarounds: Program name: "AIO-Graphics-Test.exe" (hash: a88877c11331fe8d)
698083.265:00e8:00ec:info:vkd3d-proton:vkd3d_instance_deduce_config_flags_from_environment: shader_cache is used, global_pipeline_cache is enforced.
698083.265:00e8:00ec:info:vkd3d-proton:vkd3d_config_flags_init_once: VKD3D_CONFIG=''.
698083.271:00e8:00ec:info:vkd3d-proton:vkd3d_get_vk_version: vkd3d-proton - applicationVersion: 2.14.1.
698083.271:00e8:00ec:info:vkd3d-proton:vkd3d_instance_init: vkd3d-proton - build: 0d66699b1b1e250.
698083.351:00e8:00ec:info:vkd3d-proton:vkd3d_init_device_caps: Not all relevant pipeline stages are supported by EXT_dgc. Skipping EXT.
[info]: Adding extension VK_EXT_device_fault
[info]: Enabling VK_EXT_device_fault features
[info]: Enabling storageBuffer8BitAccess
[info]: Enabling storageBuffer16BitAccess
[info]: Enabling shaderFloat16 and shaderInt8
[info]: Masking extension: VK_EXT_vertex_attribute_divisor
[info]: Masking feature pipelineStatisticsQuery in VkPhysicalDeviceFeatures
[info]: Masking feature shaderResourceMinLod in VkPhysicalDeviceFeatures
[info]: Masking feature shaderResourceResidency in VkPhysicalDeviceFeatures
[info]: Masking feature sparseBinding in VkPhysicalDeviceFeatures
[info]: Masking feature sparseResidencyAliased in VkPhysicalDeviceFeatures
[info]: Masking feature sparseResidencyBuffer in VkPhysicalDeviceFeatures
[info]: Masking feature sparseResidencyImage2D in VkPhysicalDeviceFeatures
[info]: Masking feature vertexPipelineStoresAndAtomics in VkPhysicalDeviceFeatures
[info]: Masking feature vertexAttributeInstanceRateZeroDivisor in VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT
[info]: Masking feature geometryStreams in VkPhysicalDeviceTransformFeedbackFeaturesEXT
[info]: Masking feature robustBufferAccess2 in VkPhysicalDeviceRobustness2FeaturesEXT
[info]: Masking feature robustImageAccess2 in VkPhysicalDeviceRobustness2FeaturesEXT
[info]: Dispatch not found: vkCmdDrawIndirectCountAMD / vkCmdDrawIndirectCountAMDKHR
[info]: Dispatch not found: vkCmdDrawIndexedIndirectCountAMD / vkCmdDrawIndexedIndirectCountAMDKHR
[info]: Dispatch not found: vkCreateSharedSwapchainsKHR / vkCreateSharedSwapchainsKHRKHR
[info]: Dispatch not found: vkDebugMarkerSetObjectTagEXT / vkDebugMarkerSetObjectTagEXTKHR
[info]: Dispatch not found: vkDebugMarkerSetObjectNameEXT / vkDebugMarkerSetObjectNameEXTKHR
[info]: Dispatch not found: vkCmdDebugMarkerBeginEXT / vkCmdDebugMarkerBeginEXTKHR
[info]: Dispatch not found: vkCmdDebugMarkerEndEXT / vkCmdDebugMarkerEndEXTKHR
[info]: Dispatch not found: vkCmdDebugMarkerInsertEXT / vkCmdDebugMarkerInsertEXTKHR
[info]: Dispatch not found: vkDeviceSetApiDumpState / vkDeviceSetApiDumpStateKHR
698084.125:00e8:00ec:info:vkd3d-proton:vkd3d_memory_info_decide_hvv_usage: Topology: UMA-like topology.
698084.126:00e8:00ec:info:vkd3d-proton:vkd3d_memory_info_upload_hvv_memory_properties: Topology: HVV usage is allowed, using DEVICE_LOCAL | HOST_COHERENT for UPLOAD.
698084.129:00e8:00ec:info:vkd3d-proton:vkd3d_bindless_state_get_bindless_flags: Device supports VK_EXT_mutable_descriptor_type.
698084.129:00e8:00ec:info:vkd3d-proton:vkd3d_bindless_state_add_binding: Device supports VK_EXT_descriptor_buffer!
698084.129:00e8:00ec:info:vkd3d-proton:vkd3d_bindless_state_add_binding: Device supports VK_EXT_descriptor_buffer!
698084.242:00e8:00ec:info:vkd3d-proton:d3d12_device_caps_init_shader_model: Enabling support for SM 6.6.
698084.243:00e8:00ec:info:vkd3d-proton:d3d12_device_caps_init_shader_model: Enabling support for SM 6.7.
698084.243:00e8:00ec:info:vkd3d-proton:d3d12_device_caps_init_shader_model: Enabling support for SM 6.8.
698084.244:00e8:00ec:fixme:vkd3d-proton:d3d12_device_caps_init_feature_options1: TotalLaneCount = 512, may be inaccurate.
698084.246:00e8:00ec:info:vkd3d-proton:vkd3d_pipeline_library_init_disk_cache: Remapping VKD3D_SHADER_CACHE to: vkd3d-proton.cache.
698084.246:00e8:00ec:info:vkd3d-proton:vkd3d_pipeline_library_init_disk_cache: Attempting to load disk cache from: vkd3d-proton.cache.
warn:  CreateDXGIFactory2: Ignoring flags
info:  Wrapper(Mali-G715):
info:    Driver : Wrapper driver 54.2.0
info:    Memory Heap[0]:
info:      Size: 11157 MiB
info:      Flags: 0x1
info:      Memory Type[0]: Property Flags = 0x7
info:      Memory Type[1]: Property Flags = 0xb
info:      Memory Type[2]: Property Flags = 0x11
info:    Memory Heap[1]:
info:      Size: 100 MiB
info:      Flags: 0x1
info:      Memory Type[3]: Property Flags = 0x21
warn:  DXGI: Found monitors not associated with any adapter, using fallback
698084.252:00e8:00ec:info:vkd3d-proton:dxgi_vk_swap_chain_init: Creating swapchain (632 x 454), BufferCount = 2.
698084.257:00e8:00ec:info:vkd3d-proton:dxgi_vk_swap_chain_init_sync_objects: Ensure maximum latency of 3 frames with KHR_present_wait.
698084.257:00e8:00f4:info:vkd3d-proton:vkd3d_pipeline_library_disk_thread_main: Performing async setup of stream archive ...
698084.263:00e8:00ec:info:vkd3d-proton:dxgi_vk_swap_chain_init_waiter_thread: Enabling present wait path for frame latency.
698084.263:00e8:00ec:info:vkd3d-proton:dxgi_vk_swap_chain_init_sleep_state: Timer interval is 1.0 ms.
698084.280:00e8:00f4:info:vkd3d-proton:vkd3d_pipeline_library_disk_cache_merge: No write cache exists. No need to merge any disk caches.
698084.300:00e8:00f4:info:vkd3d-proton:vkd3d_pipeline_library_disk_cache_initial_setup: Merging pipeline libraries took 40.534 ms.
698084.311:00e8:00f4:info:vkd3d-proton:vkd3d_pipeline_library_disk_cache_initial_setup: Failed to map read-only cache: vkd3d-proton.cache.
698084.311:00e8:00f4:info:vkd3d-proton:vkd3d_pipeline_library_disk_thread_main: Done performing async setup of stream archive.
698084.494:00e8:00f4:info:vkd3d-proton:vkd3d_pipeline_library_disk_thread_main: Pipeline cache marked dirty. Flush is scheduled.
err:   readMonitorEdidFromKey: Failed to get EDID reg key size
err:   DXGI: Failed to parse display metadata + colorimetry info, using blank.
698084.612:00e8:00fc:info:vkd3d-proton:dxgi_vk_swap_chain_recreate_swapchain_in_present_task: Got 3 swapchain images.
698084.636:00e8:00fc:info:vkd3d-proton:dxgi_vk_swap_chain_recreate_swapchain_in_present_task: Got 3 swapchain images.
698085.495:00e8:00f4:info:vkd3d-proton:vkd3d_pipeline_library_disk_thread_main: Flushing disk cache (wakeup counter since last flush = 2). It seems like application has stopped creating new PSOs for the time being.

```
