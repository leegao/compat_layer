#include "layer.hpp"
#include "logger.hpp"
#include "spoof_profile.hpp"
#include "vulkan/vk_layer.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vulkan/vulkan.h>

template <typename ValueType = std::uint32_t, typename StructType>
constexpr auto *get_field(StructType *base, std::size_t offset) noexcept {
    using ByteType = std::conditional_t<std::is_const_v<StructType>,
                                        const std::byte, std::byte>;
    using ReturnType = std::conditional_t<std::is_const_v<StructType>,
                                          const ValueType, ValueType>;
    return reinterpret_cast<ReturnType *>(reinterpret_cast<ByteType *>(base) +
                                          offset);
}

void spoof_core_features(VkPhysicalDeviceFeatures *pFeatures) {
    if (static bool logged_core = false; !logged_core) {
        logged_core = true;
        for (const auto &[sType, offset, structName, fieldName] :
             SPOOFED_CORE_FEATURES) {
            if (*get_field(pFeatures, offset) == VK_FALSE) {
                Logger::log("info",
                            "Spoofing core feature %.*s: reporting = VK_TRUE, "
                            "driver = VK_FALSE",
                            static_cast<int>(fieldName.size()),
                            fieldName.data());
            }
        }
    }

    for (const auto &[sType, offset, structName, fieldName] :
         SPOOFED_CORE_FEATURES) {
        *get_field(pFeatures, offset) = VK_TRUE;
    }
}

void spoof_next_features(VkBaseOutStructure *next) {
    static std::unordered_set<VkStructureType> logged_structs;

    for (; next != nullptr; next = next->pNext) {
        bool log_struct =
            logged_structs.find(next->sType) == logged_structs.end();

        for (const auto &[sType, offset, structName, fieldName] :
             SPOOFED_NEXT_FEATURES) {
            if (next->sType != sType)
                continue;

            auto *val_ptr = get_field(next, offset);
            if (log_struct && *val_ptr == VK_FALSE) {
                Logger::log(
                    "info",
                    "Spoofing feature %.*s in %.*s: reporting = VK_TRUE, "
                    "driver = VK_FALSE",
                    static_cast<int>(fieldName.size()), fieldName.data(),
                    static_cast<int>(structName.size()), structName.data());
            }
            *val_ptr = VK_TRUE;
        }

        if (log_struct) {
            logged_structs.insert(next->sType);
        }
    }
}

void spoof_core_properties(VkPhysicalDeviceProperties *pProperties) {
    if (static bool logged_core_props = false; !logged_core_props) {
        logged_core_props = true;
        for (const auto &[sType, offset, is_bool, val, structName, fieldName] :
             SPOOFED_CORE_PROPERTIES) {
            if (uint32_t *val_ptr = get_field(pProperties, offset);
                *val_ptr != val) {
                Logger::log(
                    "info",
                    "Spoofing core property %.*s: reporting = %u, driver = %u",
                    static_cast<int>(fieldName.size()), fieldName.data(), val,
                    *val_ptr);
            }
        }
    }

    for (const auto &[sType, offset, is_bool, val, structName, fieldName] :
         SPOOFED_CORE_PROPERTIES) {
        *get_field(pProperties, offset) = val;
    }
}

void spoof_properties(VkBaseOutStructure *next) {
    static std::unordered_set<VkStructureType> logged_structs;
    for (; next != nullptr; next = next->pNext) {
        bool log_struct =
            logged_structs.find(next->sType) == logged_structs.end();

        for (const auto &[sType, offset, is_bool, val, structName, fieldName] :
             SPOOFED_PROPERTIES) {
            if (next->sType != sType)
                continue;

            if (auto *val_ptr = get_field(next, offset); *val_ptr != val) {
                if (log_struct) {
                    Logger::log("info",
                                "Spoofing property %.*s in %.*s: reporting = "
                                "%u, driver = %u",
                                static_cast<int>(fieldName.size()),
                                fieldName.data(),
                                static_cast<int>(structName.size()),
                                structName.data(), val, *val_ptr);
                }
                *val_ptr = val;
            }
        }

        if (log_struct) {
            logged_structs.insert(next->sType);
        }
    }
}

void mask_core_features(VkPhysicalDeviceFeatures *requested,
                        const VkPhysicalDeviceFeatures *actual) {
    if (!requested || !actual)
        return;

    for (const auto &[sType, offset, structName, fieldName] :
         SPOOFED_CORE_FEATURES) {
        auto *req_ptr = get_field(requested, offset);
        const auto *act_ptr = get_field(actual, offset);
        if (*req_ptr && !(*act_ptr)) {
            Logger::log("info", "Masking feature %.*s in %.*s",
                        static_cast<int>(fieldName.size()), fieldName.data(),
                        static_cast<int>(structName.size()), structName.data());
            *req_ptr = VK_FALSE;
        }
    }
}

void mask_next_features(VkPhysicalDevice physicalDevice,
                        VkBaseOutStructure *next) {
    auto instanceKey = GetInstanceKey(physicalDevice);
    if (!instanceKey)
        return;

    auto GetPhysicalDeviceFeatures2_fn =
        instanceDispatch[instanceKey].GetPhysicalDeviceFeatures2;
    if (!GetPhysicalDeviceFeatures2_fn)
        return;

    for (; next != nullptr; next = next->pNext) {
        if (next->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
            auto *features2 =
                reinterpret_cast<VkPhysicalDeviceFeatures2 *>(next);
            VkPhysicalDeviceFeatures actualCore{};
            instanceDispatch[instanceKey].GetPhysicalDeviceFeatures(
                physicalDevice, &actualCore);
            mask_core_features(&features2->features, &actualCore);
            mask_next_features(
                physicalDevice,
                reinterpret_cast<VkBaseOutStructure *>(features2->pNext));
        }

        for (const auto &[sType, offset, structName, fieldName] :
             SPOOFED_NEXT_FEATURES) {
            if (next->sType != sType)
                continue;

            if (auto *req_ptr = get_field(next, offset); *req_ptr) {
                alignas(64) char buffer[512] = {0};
                auto *temp_struct =
                    reinterpret_cast<VkBaseOutStructure *>(buffer);
                temp_struct->sType = sType;

                VkPhysicalDeviceFeatures2 query{
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, temp_struct};
                GetPhysicalDeviceFeatures2_fn(physicalDevice, &query);

                if (const auto *act_ptr = get_field(buffer, offset);
                    !(*act_ptr)) {
                    Logger::log(
                        "info", "Masking feature %.*s in %.*s",
                        static_cast<int>(fieldName.size()), fieldName.data(),
                        static_cast<int>(structName.size()), structName.data());
                    *req_ptr = VK_FALSE;
                }
            }
        }
    }
}

inline std::unordered_set<std::string>
get_actual_device_extensions(VkPhysicalDevice physicalDevice) {
    auto key = GetKey(physicalDevice);
    if (auto it = deviceExtensionsMap.find(key);
        it != deviceExtensionsMap.end()) {
        return it->second;
    }

    auto instanceKey = GetInstanceKey(physicalDevice);
    if (!instanceKey)
        return {};

    auto &table = instanceDispatch[instanceKey];
    uint32_t count = 0;
    table.EnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count,
                                             nullptr);

    std::vector<VkExtensionProperties> extProps(count);
    table.EnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count,
                                             extProps.data());

    std::unordered_set<std::string> actual;
    actual.reserve(count);
    for (const auto &ext : extProps) {
        actual.insert(ext.extensionName);
    }
    deviceExtensionsMap[key] = actual;
    return actual;
}

std::vector<VkExtensionProperties>
get_unsupported_spoofed_extensions(VkPhysicalDevice physicalDevice) {
    auto actual = get_actual_device_extensions(physicalDevice);
    std::vector<VkExtensionProperties> result;
    static bool logged_exts = false;

    for (const auto &[name, specVersion] : SPOOFED_EXTENSIONS) {
        if (actual.find(std::string(name)) == actual.end()) {
            bool dup = false;
            for (const auto &existing : result) {
                if (name == existing.extensionName) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                VkExtensionProperties ext{};
                std::strncpy(
                    ext.extensionName, name.data(),
                    std::min(sizeof(ext.extensionName) - 1, name.size()));
                ext.specVersion = specVersion;
                result.push_back(ext);
                if (!logged_exts) {
                    Logger::log("info", "Spoofing extension %.*s: version %d",
                                static_cast<int>(name.size()), name.data(),
                                specVersion);
                }
            }
        }
    }
    logged_exts = true;
    return result;
}
