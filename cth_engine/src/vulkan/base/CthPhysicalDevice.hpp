#pragma once
#include "CthQueueFamily.hpp"
#include "vulkan/utility/cth_constants.hpp"


#include "vulkan/utility/device/PhysicalDeviceFeatures.hpp"

#include<cth/cth_pointer.hpp>
#include <vulkan/vulkan.h>

#include <memory>
#include <span>
#include <string>
#include <vector>





namespace cth::vk {
class Queue;
class Instance;
class Surface;

class PhysicalDevice {
public:
    static std::optional<PhysicalDevice> Create(VkPhysicalDevice device, Instance const* instance, Surface const& surface,
        std::span<Queue const> queues, std::span<std::string const> required_extensions,
        utils::PhysicalDeviceFeatures const& required_features);
    /**
     * @brief enumerates all available devices and picks one that fits the requirements
     * @param queues required queues to support
     * @return valid physical device
     * @throws cth::except::default_exception if no device is found
     * @note engine required features and extensions are added to the requirements
     * @link cth::vk::constants::REQUIRED_DEVICE_FEATURES
     * @link cth::vk::constants::REQUIRED_DEVICE_EXTENSIONS
     */
    [[nodiscard]] static std::unique_ptr<PhysicalDevice> AutoPick(Instance const* instance, std::span<Queue const> queues,
        std::span<std::string const> required_extensions, utils::PhysicalDeviceFeatures const& required_features);


    /**
     * @brief evaluates if the device has minimal support
     */
    [[nodiscard]] bool suitable(std::span<Queue const> queues);

    /**
     * @brief enumerates all available physical devices
     */
    [[nodiscard]] static std::vector<VkPhysicalDevice> enumerateVkDevices(Instance const& instance);

    /**
     * @return indices of missing features from utils::deviceFeaturesToArray
     */
    [[nodiscard]] std::vector<std::variant<size_t, VkStructureType>> supports(utils::PhysicalDeviceFeatures const& required_features) const;

    /**
     * @return missing extensions 
     */
    [[nodiscard]] std::vector<std::string> supports(std::span<std::string const> required_extensions);

    [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags mem_properties) const;
    [[nodiscard]] VkFormat findSupportedFormat(std::span<VkFormat const> candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

    /**
     * @brief finds a combination of queue families that support the requested queue types
     * @param queues requested queue types
     * @return order of queue types preserved
     * @return empty if no combination is possible
     */
    [[nodiscard]] std::vector<uint32_t> queueFamilyIndices(std::span<Queue const> queues) const;



    [[nodiscard]] bool supportsQueueSet(std::span<Queue const> queues) const;

private:
    explicit PhysicalDevice(VkPhysicalDevice device, Instance const* instance, Surface const& surface,
        utils::PhysicalDeviceFeatures const& required_features,
        std::span<std::string const> required_extensions);


    void setExtensions();
    void setProperties();
    void setQueueFamilyProperties(Surface const& surface);
    void setMaxSampleCount();
    void setConstants(Surface const& surface);


    move_ptr<VkPhysicalDevice_T> _vkDevice;
    Instance const* _instance;

    utils::PhysicalDeviceFeatures _features;
    utils::PhysicalDeviceFeatures _requiredFeatures;

    std::vector<std::string> _extensions{};
    std::vector<std::string> _requiredExtensions{};

    VkPhysicalDeviceProperties _properties{};
    VkPhysicalDeviceMemoryProperties _memProperties{};
    std::vector<QueueFamily> _queueFamilies{};
    VkSampleCountFlagBits _maxSampleCount = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

public:
    [[nodiscard]] VkPhysicalDevice get() const { return _vkDevice.get(); }
    [[nodiscard]] auto const& features() const { return _features; }
    [[nodiscard]] auto const& requiredFeatures() const { return _requiredFeatures; }
    [[nodiscard]] std::span<std::string const> extensions() const { return _extensions; }
    [[nodiscard]] std::span<std::string const> requiredExtensions() const { return _requiredExtensions; }
    [[nodiscard]] auto const& properties() const { return _properties; }
    [[nodiscard]] auto const& memProperties() const { return _memProperties; }
    [[nodiscard]] VkSampleCountFlagBits maxSampleCount() const { return _maxSampleCount; }
    [[nodiscard]] VkPhysicalDeviceLimits const& limits() const { return _properties.limits; }


    PhysicalDevice(PhysicalDevice const& other) = delete;
    PhysicalDevice(PhysicalDevice&& other) noexcept = default;
    PhysicalDevice& operator=(PhysicalDevice const& other) = delete;
    PhysicalDevice& operator=(PhysicalDevice&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(PhysicalDevice const* device);
    static void debug_check_handle(VkPhysicalDevice vk_device);
#define DEBUG_CHECK_PHYSICAL_DEVICE_HANDLE(vk_device) PhysicalDevice::debug_check_handle(vk_device)
#define DEBUG_CHECK_PHYSICAL_DEVICE(device_ptr) PhysicalDevice::debug_check(device_ptr)
#else
#define DEBUG_CHECK_PHYSICAL_DEVICE_HANDLE(vk_device) ((void)0)
#define DEBUG_CHECK_PHYSICAL_DEVICE(device_ptr) ((void)0)
#endif

};
}
