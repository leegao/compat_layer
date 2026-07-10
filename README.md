# compat_layer

Without push descriptors support on vkd3d 2.14:

```
vkd3d_init_device_caps: Push descriptors are not supported by this implementation. This is required for correct operation
```

same on vkd3d 2.12 and 2.9

```
13828.543:00e0:00e4:err:vkd3d-proton:vkd3d_init_device_caps: Push descriptors are not supported by this implementation. This is required for correct operation.
```

silent crash on 2.8

---

Difference between Mali v1.r44p1 and v1.r54p3:

```
* VK_EXT_depth_clip_enable (Required version: 1)
* VK_EXT_robustness2 (Required version: 1)
* VK_KHR_push_descriptor (Required version: 1) <<<
* VkPhysicalDeviceDepthClipEnableFeaturesEXT (missing feature struct) -> depthClipEnable
* VkPhysicalDeviceFeatures -> dualSrcBlend
* VkPhysicalDeviceFeatures -> logicOp
* VkPhysicalDeviceRobustness2FeaturesEXT (missing feature struct):
    * robustBufferAccess2
    * robustImageAccess2
    * nullDescriptor
* VkPhysicalDevicePushDescriptorPropertiesKHR -> maxPushDescriptors: Not specified/supported by device (Requires 32)
* VkPhysicalDeviceVulkan12Properties:
    * maxPerStageDescriptorUpdateAfterBindStorageBuffers: Device value 500000 is less than required minimum of 1000000
    * maxPerStageDescriptorUpdateAfterBindSampledImages: Device value 500000 is less than required minimum of 1000000
    * maxPerStageDescriptorUpdateAfterBindStorageImages: Device value 500000 is less than required minimum of 1000000
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
