#include "layer.hpp"
#include "logger.hpp"
#include <vulkan/vulkan.h>

VK_LAYER_EXPORT VkResult VKAPI_CALL
DxvkMaliCompatLayer_GetPhysicalDeviceCalibrateableTimeDomainsKHR(
    VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount,
    VkTimeDomainKHR *pTimeDomains) {
    static_assert(std::is_same_v<VkTimeDomainEXT, VkTimeDomainKHR>);

    Logger::log("info",
                "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR called!");
    const auto &dispatch = ({
        scoped_lock l(global_lock);
        auto instanceKey = GetInstanceKey(physicalDevice);
        if (!instanceKey)
            return VK_ERROR_INITIALIZATION_FAILED;
        instanceDispatch[instanceKey];
    });

    if (dispatch.GetPhysicalDeviceCalibrateableTimeDomainsEXT) {
        return dispatch.GetPhysicalDeviceCalibrateableTimeDomainsEXT(
            physicalDevice, pTimeDomainCount,
            reinterpret_cast<VkTimeDomainEXT *>(pTimeDomains));
    }

    if (dispatch.GetPhysicalDeviceCalibrateableTimeDomainsKHR) {
        return dispatch.GetPhysicalDeviceCalibrateableTimeDomainsKHR(
            physicalDevice, pTimeDomainCount, pTimeDomains);
    }

    Logger::log(
        "error",
        "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR/EXT not available");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DxvkMaliCompatLayer_GetCalibratedTimestampsKHR(
    VkDevice device, uint32_t timestampCount,
    const VkCalibratedTimestampInfoKHR *pTimestampInfos, uint64_t *pTimestamps,
    uint64_t *pMaxDeviation) {
    static_assert(std::is_same_v<VkCalibratedTimestampInfoEXT,
                                 VkCalibratedTimestampInfoKHR>);

    struct device *dev = get_device(device);
    if (!dev)
        return VK_ERROR_DEVICE_LOST;

    if (dev->table.GetCalibratedTimestampsKHR) {
        return dev->table.GetCalibratedTimestampsKHR(
            device, timestampCount, pTimestampInfos, pTimestamps,
            pMaxDeviation);
    }

    if (dev->table.GetCalibratedTimestampsEXT) {
        return dev->table.GetCalibratedTimestampsEXT(
            device, timestampCount,
            reinterpret_cast<const VkCalibratedTimestampInfoEXT *>(
                pTimestampInfos),
            pTimestamps, pMaxDeviation);
    }

    Logger::log("error", "vkGetCalibratedTimestampsKHR/EXT not available");
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}
