#pragma once
#include "CthQueueFamily.hpp"
#include "vulkan/utility/CthConstants.hpp"


#include<cth/cth_pointer.hpp>

#include <vulkan/vulkan.h>

#include <array>
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
    explicit PhysicalDevice(VkPhysicalDevice device, const Instance* instance, const Surface& surface);


    /**
     * @brief evaluates if the device has minimal support
     */
    [[nodiscard]] bool suitable(const VkPhysicalDeviceFeatures& features, std::span<const std::string_view> extensions,
        std::span<const Queue> queues);

    /**
     * @brief enumerates all available physical devices
     */
    [[nodiscard]] static std::vector<VkPhysicalDevice> enumerateDevices(const Instance& instance);
    [[nodiscard]] static std::unique_ptr<PhysicalDevice> autoPick(const Instance* instance, const VkPhysicalDeviceFeatures& features,
        std::span<const std::string_view> extensions, std::span<VkPhysicalDevice_T* const> devices, std::span<const Queue> queues);

    /**
     * @brief auto picks a physical device that has at least minimal support
     * @return a unique ptr with the created device
     */
    [[nodiscard]] static std::unique_ptr<PhysicalDevice> autoPick(const Instance& instance, const std::span<const Queue> queues);

    /**
     * @return indices of missing features from utils::deviceFeaturesToArray
     */
    [[nodiscard]] std::vector<uint32_t> supports(const VkPhysicalDeviceFeatures& required_features) const;

    /**
     * @return missing extensions 
     */
    [[nodiscard]] std::vector<std::string> supports(std::span<const std::string_view> required_extensions);

    [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags mem_properties) const;
    [[nodiscard]] VkFormat findSupportedFormat(std::span<const VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

    /**
     * @brief finds a combination of queue families that support the requested queue types
     * @param queues requested queue types
     * @return order of queue types preserved
     * @return empty if no combination is possible
     */
    [[nodiscard]] std::vector<uint32_t> queueFamilyIndices(std::span<const Queue> queues) const;



    [[nodiscard]] bool supportsQueueSet(std::span<const Queue> queues) const;

private:
    void setFeatures();
    void setExtensions();
    void setProperties();
    void setQueueFamilyProperties(const Surface& surface);
    void setMaxSampleCount();
    void setConstants(const Surface& surface);


    move_ptr<VkPhysicalDevice_T> _vkDevice;
    const Instance* _instance;

    VkPhysicalDeviceFeatures _features{};
    std::vector<std::string> _extensions{};
    VkPhysicalDeviceProperties _properties{};
    VkPhysicalDeviceMemoryProperties _memProperties{};
    std::vector<QueueFamily> _queueFamilies{};
    VkSampleCountFlagBits _maxSampleCount = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

public:
    [[nodiscard]] VkPhysicalDevice get() const { return _vkDevice.get(); }
    [[nodiscard]] const auto& features() const { return _features; }
    [[nodiscard]] std::span<const std::string> extensions() const { return _extensions; }
    [[nodiscard]] const auto& properties() const { return _properties; }
    [[nodiscard]] const auto& memProperties() const { return _memProperties; }
    [[nodiscard]] VkSampleCountFlagBits maxSampleCount() const { return _maxSampleCount; }
    [[nodiscard]] const VkPhysicalDeviceLimits& limits() const { return _properties.limits; }


    static constexpr std::array<std::string_view, 3> REQUIRED_DEVICE_EXTENSIONS = {
        std::string_view(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
        std::string_view(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME),
        std::string_view(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME),
    };

    static constexpr VkPhysicalDeviceFeatures REQUIRED_DEVICE_FEATURES{
        .samplerAnisotropy = true,
    };
    static constexpr VkPhysicalDeviceTimelineSemaphoreFeatures TIMELINE_SEMAPHORE_FEATURE{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
        .timelineSemaphore = true
    };



    static constexpr VkPhysicalDeviceFeatures2 REQUIRED_DEVICE_FEATURES2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
        .pNext = (void*) (&TIMELINE_SEMAPHORE_FEATURE),
        .features = REQUIRED_DEVICE_FEATURES,
    };


#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const PhysicalDevice* device);
    static void debug_check_handle(VkPhysicalDevice vk_device);
#define DEBUG_CHECK_PHYSICAL_DEVICE_HANDLE(vk_device) PhysicalDevice::debug_check_handle(vk_device)
#define DEBUG_CHECK_PHYSICAL_DEVICE(device_ptr) PhysicalDevice::debug_check(device_ptr)
#else
#define DEBUG_CHECK_PHYSICAL_DEVICE_HANDLE(vk_device) ((void)0)
#define DEBUG_CHECK_PHYSICAL_DEVICE(device_ptr) ((void)0)
#endif

};
}
