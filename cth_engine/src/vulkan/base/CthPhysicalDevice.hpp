#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace cth {
using namespace std;

class Instance;
class Surface;

class PhysicalDevice {
public:
    explicit PhysicalDevice(VkPhysicalDevice device);


    [[nodiscard]] bool suitable(const VkPhysicalDeviceFeatures& features, span<const string> extensions, const Surface* surface, span<const VkQueueFlagBits> queue_families);

    [[nodiscard]] static vector<VkPhysicalDevice> enumerateDevices(const Instance* instance);
    [[nodiscard]] static unique_ptr<PhysicalDevice> autoPick(const VkPhysicalDeviceFeatures& features, span<const string> extensions,
        span<const VkQueueFlagBits> queue_families, const Surface* surface, span<const VkPhysicalDevice> devices);
    [[nodiscard]] static unique_ptr<PhysicalDevice> autoPick(const Surface* surface, const Instance* instance);
    /**
     * \return indices of missing features from utils::deviceFeaturesToArray
     */

    [[nodiscard]] vector<uint32_t> supports(const VkPhysicalDeviceFeatures& required_features);

    /**
     * \return missing extensions 
     */
    [[nodiscard]] vector<string> supports(span<const string> required_extensions);

    [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags mem_properties) const;
    [[nodiscard]] VkFormat findSupportedFormat(span<const VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

    /**
     * \brief finds a combination of queue families that support the requested queue types
     * \param surface surface != nullptr => present queue index at result[0]
     * \param requested_queues requested queue types
     * \return order of queue types preserved (present queue index at result[0])
     */
    [[nodiscard]] vector<uint32_t> queueFamilyIndices(const Surface* surface, span<const VkQueueFlagBits> requested_queues);
    /**
     * \throws cth::except::vk_result_exception data: VkResult from vkGetPhysicalDeviceSurfaceSupportKHR()
     */
    [[nodiscard]] bool supportsPresentQueue(const Surface* surface, uint32_t family_index) const;

    [[nodiscard]] vector<VkPresentModeKHR> supportedPresentModes(const Surface* surface) const;
    [[nodiscard]] vector<VkSurfaceFormatKHR> supportedFormats(const Surface* surface) const;
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(const Surface* surface) const;
    [[nodiscard]] VkBool32 supportsFamily(const Surface* physical_device, uint32_t family_index) const;


private:
    void setFeatures();
    void setExtensions();
    void setProperties();
    void setMaxSampleCount();
    void setConstants();

    VkPhysicalDevice vkDevice;
    VkPhysicalDeviceFeatures _features;
    vector<string> _extensions{};
    VkPhysicalDeviceProperties _properties;
    VkPhysicalDeviceMemoryProperties _memProperties;
    vector<VkQueueFamilyProperties> queueFamilies{};
    VkSampleCountFlagBits _maxSampleCount;

public:
    [[nodiscard]] VkPhysicalDevice get() const { return vkDevice; }
    [[nodiscard]] const auto& features() const { return _features; }
    [[nodiscard]] span<const string> extensions() const { return _extensions; }
    [[nodiscard]] const auto& properties() const { return _properties; }
    [[nodiscard]] const auto& memProperties() const { return _memProperties; }
    [[nodiscard]] VkSampleCountFlagBits maxSampleCount() const { return _maxSampleCount; }
    [[nodiscard]] const VkPhysicalDeviceLimits& limits() const { return _properties.limits; }


    inline static const array<string, 2> REQUIRED_DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};
    inline static constexpr VkPhysicalDeviceFeatures REQUIRED_DEVICE_FEATURES = []() {
        VkPhysicalDeviceFeatures features{};
        features.samplerAnisotropy = true;
        return features;
    }();
};
}
