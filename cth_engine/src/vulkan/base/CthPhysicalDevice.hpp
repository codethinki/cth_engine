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


    /**
     * \param surface if surface != nullptr a present queue will be required automatically 
     */
    [[nodiscard]] bool suitable(const VkPhysicalDeviceFeatures& features, span<const string> extensions, span<const VkQueueFlagBits> queue_families,
        const Surface* surface = nullptr);

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

    /**
     * \brief finds a combination of queue families that support the requested queue types
     * \param requested_queues requested queue types
     * \param surface optional present queue
     * \return indices of the requested queue types in the same order as they were requested. The present queue is always last.
     */
    [[nodiscard]] vector<uint32_t> queueFamilyIndices(span<const VkQueueFlagBits> requested_queues, const Surface* surface = nullptr);
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
    vector<VkQueueFamilyProperties> queueFamilies{};
    VkSampleCountFlagBits _maxSampleCount;

public:
    [[nodiscard]] VkPhysicalDevice get() const { return vkDevice; }
    [[nodiscard]] const auto& features() const { return _features; }
    [[nodiscard]] span<const string> extensions() const { return _extensions; }
    [[nodiscard]] const VkPhysicalDeviceProperties& properties() const { return _properties; }
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
