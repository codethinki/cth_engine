#pragma once
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/utility/device/device_features.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>


namespace cvk {
class Instance;
class Surface;

/**
 * @brief VkPhysicalDevice with its queried extensions, features, properties and queue families
 * @details copyable, the instance owns the physical device
 */
class PhysicalDevice {
public:
    using vk_handle_t = VkPhysicalDevice;

    /**
     * @brief queries the device
     * @param surfaces a family presents only if it presents to every surface, empty -> no present support
     * @pre the instance of the device is created
     * @throws cvk::vk_result_exception result of vkEnumerateDeviceExtensionProperties()
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceSupportKHR()
     */
    explicit PhysicalDevice(vk_not_null<VkPhysicalDevice>, std::span<Surface const> surfaces = {});

    ~PhysicalDevice() = default;

    /**
     * @param features to require
     * @param extensions to require, besides the extensions of the features
     * @return names of the features and the extensions that the device does not have
     */
    [[nodiscard]] std::vector<std::string> missing(
        DeviceFeatures const& features,
        std::span<std::string const> extensions = {}
    ) const;

    /**
     * @brief finds queue families for the requested queues
     * @param queueProperties requested queues
     * @return family index per requested queue, empty -> no combination
     */
    [[nodiscard]] std::vector<queue_family_index_t> queueFamilyIndices(
        std::span<QueueFamilyProperties const> queueProperties
    ) const;

    /**
     * @return first of the candidates that supports the features with the tiling
     * @throws cvk::cvk_exception reason: no candidate supports the features
     */
    [[nodiscard]] VkFormat findSupportedFormat(
        std::span<VkFormat const> candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const;

    /**
     * @param idx queue family index
     * @return nullptr -> no family with the index
     */
    [[nodiscard]] QueueFamily const* queueFamily(queue_family_index_t idx) const;

    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr std::span<std::string const> extensions() const { return _extensions; }
    [[nodiscard]] constexpr DeviceFeatures const& features() const { return _features; }
    [[nodiscard]] constexpr VkPhysicalDeviceProperties const& properties() const { return _properties; }
    [[nodiscard]] constexpr VkPhysicalDeviceLimits const& limits() const { return _properties.limits; }
    [[nodiscard]] constexpr VkPhysicalDeviceMemoryProperties const& memoryProperties() const {
        return _memoryProperties;
    }
    [[nodiscard]] constexpr VkSampleCountFlagBits maxSampleCount() const { return _maxSampleCount; }
    [[nodiscard]] constexpr std::span<QueueFamily const> queueFamilies() const { return _queueFamilies; }

private:
    move_ptr<VkPhysicalDevice_T> _handle;
    std::vector<std::string> _extensions;
    DeviceFeatures _features;
    VkPhysicalDeviceProperties _properties;
    VkPhysicalDeviceMemoryProperties _memoryProperties;
    VkSampleCountFlagBits _maxSampleCount;
    std::vector<QueueFamily> _queueFamilies;

public:
    PhysicalDevice(PhysicalDevice const& other) = default;
    PhysicalDevice(PhysicalDevice&& other) noexcept = default;
    PhysicalDevice& operator=(PhysicalDevice const& other) = default;
    PhysicalDevice& operator=(PhysicalDevice&& other) noexcept = default;

    static constexpr void debug_check(PhysicalDevice const&);
};

/**
 * @brief physical device for the requested queues, with the family index of each queue
 */
struct PhysicalDevicePick {
    PhysicalDevice device;
    std::vector<queue_family_index_t> queueFamilyIndices;
};

/**
 * @brief queries every physical device of the instance
 * @param surfaces a family presents only if it presents to every surface, empty -> no present support
 * @pre @ref Instance::created()
 * @throws cvk::vk_result_exception result of vkEnumeratePhysicalDevices()
 */
[[nodiscard]] std::vector<PhysicalDevice> enumerate_physical_devices(
    Instance const&,
    std::span<Surface const> surfaces = {}
);

/**
 * @brief picks the first physical device with the features, the extensions and the queues
 * @param surfaces a family presents only if it presents to every surface
 * @param extensions to require, besides the extensions of the features
 * @return std::nullopt -> no device meets the requirements
 * @pre @ref Instance::created()
 * @throws cvk::vk_result_exception result of vkEnumeratePhysicalDevices()
 */
[[nodiscard]] std::optional<PhysicalDevicePick> pick_physical_device(
    Instance const&,
    std::span<Surface const> surfaces,
    std::span<QueueFamilyProperties const> queueProperties,
    DeviceFeatures const& features,
    std::span<std::string const> extensions = {}
);
}

namespace cvk {
constexpr void PhysicalDevice::debug_check(PhysicalDevice const& device) {
    CTH_CRITICAL(device.get() == VK_NULL_HANDLE, "physical device must not be moved from") {}
}
}
