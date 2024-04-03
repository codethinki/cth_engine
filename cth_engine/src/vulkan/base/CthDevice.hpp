#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>


namespace cth {
using namespace std;
class Instance;
class Window;

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    vector<VkSurfaceFormatKHR> formats;
    vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    uint32_t graphicsFamilyIndex = MAX;
    uint32_t presentFamilyIndex = MAX;
    [[nodiscard]] bool graphicsFamily() const { return graphicsFamilyIndex != MAX; }
    [[nodiscard]] bool presentFamily() const { return presentFamilyIndex != MAX; }
    [[nodiscard]] bool complete() const { return graphicsFamily() && presentFamily() && graphicsFamilyIndex != presentFamilyIndex; }

private:
    static constexpr uint32_t MAX = numeric_limits<uint32_t>::max();
};

class Device {
public:
    static constexpr array<const char*, 2> REQUIRED_DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};
    static constexpr VkPhysicalDeviceFeatures REQUIRED_DEVICE_FEATURES = []() {
        VkPhysicalDeviceFeatures features{};
        features.samplerAnisotropy = true;
        return features;
    }();

    explicit Device(Window* window, Instance* instance);
    ~Device();

    [[nodiscard]] SwapchainSupportDetails getSwapchainSupport() const { return querySwapchainSupport(vkPhysicalDevice); }
    /**
     * \throws cth::except::default_exception reason: no suitable memory type
     */
    [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] QueueFamilyIndices findPhysicalQueueFamilies() const { return findQueueFamilies(vkPhysicalDevice); }
    /**
     *\throws cth::except::data_exception data: features param
     */
    [[nodiscard]] VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    [[nodiscard]] VkSampleCountFlagBits evaluateMaxUsableSampleCount() const;

    // Buffer Helper Functions

    /**
     * \throws cth::except::vk_result_exception result of vkCreateBuffer()
     * \throws cth::except::vk_result_exception result of vkAllocateMemory()
     */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& buffer_memory) const;

    [[nodiscard]] VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer command_buffer) const;
    //TODO why is this here
    /**
     * \param size in bytes 
     * \param src_offset in bytes
     * \param dst_offset in bytes
     */
    void copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size, VkDeviceSize src_offset = 0, VkDeviceSize dst_offset = 0) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layer_count) const;

    //TODO put this into the image class maybe
    /**
     * \throws cth::except::vk_result_exception result of vkCreateImage()
     * \throws cth::except::vk_result_exception result of vkAllocateMemory()
     * \throws cth::except::vk_result_exception result of vkBindImageMemory()
     */
    void createImageWithInfo(const VkImageCreateInfo& image_info, VkMemoryPropertyFlags properties, VkImage& image,
        VkDeviceMemory& image_memory) const;

private:
    //pickPhysicalDevice
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physical_device) const;
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice physical_device) const;
    [[nodiscard]] vector<string> checkDeviceExtensionSupport(VkPhysicalDevice physical_device) const;
    [[nodiscard]] vector<uint32_t> checkDeviceFeatureSupport(const VkPhysicalDevice& device) const;
    [[nodiscard]] bool physicalDeviceSuitable(VkPhysicalDevice physical_device) const;
    /**
     * \throws cth::except::default_exception reason: no vulkan gpus
     * \throws cth::except::default_exception reason: no suitable gpu
     */
    void pickPhysicalDevice();

    void setDeviceSpecificConstants();

    //createLogicalDevice
    /**
    * \throws cth::except::vk_result_exception result of vkCreateDevice()
    */
    void createLogicalDevice();

    //createCommandPool
    /**
     * \throws cth::except::vk_result_exception result of vkCreateCommandPool()
     */
    void createCommandPool();

    Window* window;
    Instance* instance;


    VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    VkDevice vkDevice = VK_NULL_HANDLE;
    VkQueue vkGraphicsQueue = VK_NULL_HANDLE;
    VkQueue vkPresentQueue = VK_NULL_HANDLE;

    uint32_t _minBufferAlignment;
    VkPhysicalDeviceProperties physicalProperties;


public:
    [[nodiscard]] VkCommandPool getCommandPool() const { return commandPool; }
    [[nodiscard]] VkDevice get() const { return vkDevice; }
    [[nodiscard]] VkQueue graphicsQueue() const { return vkGraphicsQueue; }
    [[nodiscard]] VkQueue presentQueue() const { return vkPresentQueue; }
    [[nodiscard]] VkPhysicalDeviceLimits limits() const { return physicalProperties.limits; }
    [[nodiscard]] uint32_t minBufferAlignment() const { return _minBufferAlignment; }

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;
};
} // namespace cth
