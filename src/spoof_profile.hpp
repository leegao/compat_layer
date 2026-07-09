#pragma once
#include <string>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.h>

struct SpoofedExtension {
    std::string_view name;
    uint32_t specVersion;
};

struct SpoofedFeature {
    VkStructureType sType;
    size_t offset;
    std::string_view structName;
    std::string_view fieldName;
};

struct SpoofedProperty {
    VkStructureType sType;
    size_t offset;
    bool is_bool;
    uint32_t val;
    std::string_view structName;
    std::string_view fieldName;
};

#include "generated_spoofed_profile.hpp"

void spoof_core_features(VkPhysicalDeviceFeatures *pFeatures);
void spoof_next_features(VkBaseOutStructure *next);
void spoof_core_properties(VkPhysicalDeviceProperties *pProperties);
void spoof_properties(VkBaseOutStructure *next);

void mask_core_features(VkPhysicalDeviceFeatures *requested,
                        const VkPhysicalDeviceFeatures *actual);
void mask_next_features(VkPhysicalDevice physicalDevice,
                        VkBaseOutStructure *next);

std::vector<VkExtensionProperties>
get_unsupported_spoofed_extensions(VkPhysicalDevice physicalDevice);
std::unordered_set<std::string>
get_actual_device_extensions(VkPhysicalDevice physicalDevice);
