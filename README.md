# compat_layer

Compatibility layer for Mali blob vulkan drivers to work with vkd3d 2.14.1, including:

1. emulation of `VK_EXT_push_descriptor`
2. emulation of `VK_EXT_robustness2` (specifically `nullDescriptor`)
3. emulation of `sparseBinding` (`COMPAT_EMULATE_SPARSE_BINDING`, disabled by default) for D3D 12.0 games that use tiled resources (generically missing on Mali)
4. Generic spoofing of other missing features/extensions to get vkd3d to reach D3D feature level 12.0

Minimal supported profile: **driver must be at least `r32p1`**, with the following exceptions:

1. `Mali-G72` - requires `r44p1`+ (`r38p1` does not support `VK_EXT_descriptor_indexing` on these devices)
2. `Mali-G52` and `Mali-G76` (Bifrost) - unsupported due to lack of hardware support for `VK_EXT_descriptor_indexing`, but can support Mina the Hollower on FL11 in vkd3d
3. `Mali-G51` and `Mali-G71` (Bifrost) - unsupported due to lack of Vulkan 1.2 and 1.3
4. `Mali-T830` and `Mali-T880` (Midgard) - unsupported due to lack of Vulkan 1.2 and 1.3

Note that sparse bindings is not supported generically on Mali, so d3d12 games that use tiled textures may need `COMPAT_EMULATE_SPARSE_BINDING=1` which is not enabled by default

---

## By GPU

### **1. Mali-T830**

- **Approximate Launch Date:** October, 2014 (Midgard Architecture)
- **Earliest Supported Driver:** **None**
- **Problems:**
    - Does not support Vulkan 1.3

---

### **2. Mali-T880**

- **Approximate Launch Date:** February, 2015 (Midgard Architecture Flagship)
- **Earliest Supported Driver:** **None**
- **Problems:**
    - Does not support Vulkan 1.3

---

### **3. Mali-G71**

- **Approximate Launch Date:** May, 2016 (Bifrost Architecture Flagship)
- **Earliest Supported Driver:** **None**
- **Problems:**
    - Does not support Vulkan 1.3

---

### **4. Mali-G51**

- **Approximate Launch Date:** October, 2016 (Bifrost Architecture Mid-range)
- **Earliest Supported Driver:** **None**
- **Problems:**
    - Does not support Vulkan 1.3

---

### **5. Mali-G72**

- **Approximate Launch Date:** May, 2017 (Bifrost Architecture Flagship)
- **Earliest Supported Driver:** **44.1.0** (first to support descriptor indexing)
- **Problems:**
    - **Driver 38.1.0:**: Lacks support for `VK_EXT_descriptor_indexing`

---

### **6. Mali-G31**

- **Approximate Launch Date:** March, 2018 (Bifrost Architecture Entry-level)
- **Earliest Supported Driver:** **None**
- **Problems:**
    - Does not support Vulkan 1.3

---

### **7. Mali-G52**

- **Approximate Launch Date:** March, 2018 (Bifrost Architecture Mid-range)
- **Earliest Supported Driver:** **None**
- **Problems:**
    - **Driver 49.1.0:** Lacks support for `VK_EXT_descriptor_indexing`

---

### **8. Mali-G76**

- **Approximate Launch Date:** May, 2018 (Bifrost Architecture Flagship)
- **Earliest Supported Driver:** **None**
- **Problems:**
    - **Driver 38.1.0:**: Lacks support for `VK_EXT_descriptor_indexing`

---

### **9. Mali-G77**

- **Approximate Launch Date:** May, 2019 (Valhall Architecture Flagship)
- **Earliest Supported Driver:** **32.1.0**
- **Problems:**
    - **Driver 26.0.0:** Lacks Custom Border Color and Transform Feedback.
    - **Drivers 21.0.0:** Lacks Custom Border Color, Transform Feedback, and core Vulkan 1.2 features

---

### **10. Mali-G57**

- **Approximate Launch Date:** October, 2019 (Valhall Architecture Mid-range)
- **Earliest Supported Driver:** **32.1.0** (likely 28.0.0)
- **Problems:**
    - **Driver 28.0.0:** Lacks Custom Border Color.
    - **Driver 26.0.0:** Lacks Custom Border Color and Transform Feedback.

---

### **11. Mali-G68**

- **Approximate Launch Date:** May, 2020 (Valhall Architecture Sub-premium)
- **Earliest Supported Driver:** **32.1.0**
- **Problems:**
    - **Driver 26.0.0:** Lacks Custom Border Color.

---

### **12. Mali-G78**

- **Approximate Launch Date:** May, 2020 (Valhall Architecture Flagship)
- **Earliest Supported Driver:** **32.1.0**
- **Problems:**
    - **Driver 32.1.0:** [1 device](http://vulkan.gpuinfo.org/api/v2/getreport.php?id=16265) Lacks Custom Border Color and Transform Feedback, but not seen on other profiles

---

### **13. Mali-G310**

- **Approximate Launch Date:** May, 2021 (Valhall Architecture - hardkernel ODROID only)
- **Earliest Supported Driver:** **44.0.0** (initial shipped driver)
- **Problems:** None.

---

### **14. Mali-G610**

- **Approximate Launch Date:** May, 2021 (Valhall Architecture Sub-premium)
- **Earliest Supported Driver:** **32.1.0**
- **Problems:** None.

---

### **15. Mali-G710**

- **Approximate Launch Date:** May, 2021 (Valhall Architecture Flagship)
- **Earliest Supported Driver:** **32.1.0**
- **Problems:** None.

---

### **16. Mali-G615**

- **Approximate Launch Date:** June, 2022 (Valhall Architecture Sub-premium)
- **Earliest Supported Driver:** **44.1.0** (initial shipped driver)
- **Problems:** None.

---

### **17. Mali-G715**

- **Approximate Launch Date:** June, 2022 (Valhall Architecture Flagship)
- **Earliest Supported Driver:** **38.1.0** (initial shipped driver)
- **Problems:** None.

---

### **18. Mali-G720**

- **Approximate Launch Date:** May, 2023 (5th Generation Architecture Flagship)
- **Earliest Supported Driver:** **44.1.0** (initial shipped driver)
- **Problems:** None.

---

### **19. Mali-G925**

- **Approximate Launch Date:** May, 2024 (5th Generation Architecture Flagship)
- **Earliest Supported Driver:** **49.1.0** (initial shipped driver)
- **Problems:** None.

---

### **20. Mali-G1-Ultra**

- **Approximate Launch Date:** September, 2025 (Lumex CSS Generation flagship)
- **Earliest Supported Driver:** **54.1.0** (initial shipped driver)
- **Problems:** None.

---

## Example

Example of the minimally supported profile (a G57 on `r38p1`)

```
GPU: Mali-G57 MC2
Driver: v1.r38p1-01eac0.51dbefa0fd07f79a7a6805f21ef15c50
Supported API: 1.3.219

[FAILED] Profile: VP_D3D12_FL_12_0_baseline
  Description: Minimum baseline to create a device with FL 12.0.
  Vulkan API: 1.3.219, needs at least 1.3.204
  - Missing/Insufficient Extensions:
      * [ ✓ ] VK_EXT_depth_clip_enable (Required version: 1)
      * [ ✓(emulated) ] VK_EXT_robustness2 (Required version: 1)
      * [ W ] VK_EXT_vertex_attribute_divisor (Required version: 3)
      * [ ✓(emulated) ] VK_KHR_push_descriptor (Required version: 1)
  - Missing Required Feature Flags:
      * [ ✓ ] VkPhysicalDeviceDepthClipEnableFeaturesEXT -> depthClipEnable
      * [ W ] VkPhysicalDeviceFeatures -> dualSrcBlend
      * [ W ] VkPhysicalDeviceFeatures -> multiDrawIndirect
      * [ W ] VkPhysicalDeviceFeatures -> fillModeNonSolid
      * [ W ] VkPhysicalDeviceFeatures -> multiViewport
      * [ ✓(emulated) ] VkPhysicalDeviceFeatures -> textureCompressionBC
      * [ W ] VkPhysicalDeviceFeatures -> shaderClipDistance
      * [ W ] VkPhysicalDeviceFeatures -> shaderCullDistance
      * [ ✓ ] VkPhysicalDeviceFeatures -> vertexPipelineStoresAndAtomics
      * [ ✓ ] VkPhysicalDeviceFeatures -> logicOp
      * [ ✓(emulated) ] VkPhysicalDeviceFeatures -> sparseBinding
      * [ ✓ ] VkPhysicalDeviceFeatures -> sparseResidencyAliased
      * [ ✓ ] VkPhysicalDeviceFeatures -> sparseResidencyBuffer
      * [ ✓ ] VkPhysicalDeviceFeatures -> sparseResidencyImage2D
      * [ ✓ ] VkPhysicalDeviceFeatures -> shaderResourceResidency
      * [ ✓ ] VkPhysicalDeviceFeatures -> shaderResourceMinLod
      * [ ✓ ] VkPhysicalDeviceRobustness2FeaturesEXT -> robustBufferAccess2
      * [ ✓ ] VkPhysicalDeviceRobustness2FeaturesEXT -> robustImageAccess2
      * [ ✓(emulated) ] VkPhysicalDeviceRobustness2FeaturesEXT -> nullDescriptor
      * [ ✓ ] VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT -> vertexAttributeInstanceRateDivisor
      * [ ✓ ] VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT -> vertexAttributeInstanceRateZeroDivisor
  - Failed Properties or Limits Constraints:
      * [ ✓ ] VkPhysicalDeviceProperties -> sparseProperties -> residencyStandard2DBlockShape: Device value is False (Expected True)
      * [ ✓ ] VkPhysicalDeviceProperties -> sparseProperties -> residencyNonResidentStrict: Device value is False (Expected True)
      * [ ✓(emulated) ] VkPhysicalDevicePushDescriptorPropertiesKHR -> maxPushDescriptors: Not specified/supported by device (Requires 32)
      * [ ✓ ] VkPhysicalDeviceVulkan12Properties -> maxPerStageDescriptorUpdateAfterBindStorageBuffers: Device value 500000 is less than required minimum of 1000000
      * [ ✓ ] VkPhysicalDeviceVulkan12Properties -> maxPerStageDescriptorUpdateAfterBindSampledImages: Device value 500000 is less than required minimum of 1000000
      * [ ✓ ] VkPhysicalDeviceVulkan12Properties -> maxPerStageDescriptorUpdateAfterBindStorageImages: Device value 500000 is less than required minimum of 1000000
      * [ ✓ ] VkPhysicalDeviceVulkan12Properties -> filterMinmaxSingleComponentFormats: Device value is False (Expected True)
      * [ ✓ ] VkPhysicalDeviceVulkan13Properties -> storageTexelBufferOffsetSingleTexelAlignment: Device value is False (Expected True)
  - Failed Queue Family Constraints:
      * [ ✓ ] Requires queue family with flags: ['VK_QUEUE_SPARSE_BINDING_BIT'] (Minimum Count: 1)
```
