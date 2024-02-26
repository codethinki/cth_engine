#pragma once
#include <array>

#include "../utils/HlcShader.hpp"

#include <memory>
#include <vector>

//TODO i don't like this return string if error pattern (missing log levels) use a log stream

namespace cth {

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    vector<VkSurfaceFormatKHR> formats;
    vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    uint32_t graphicsFamily{};
    uint32_t presentFamily{};
    bool graphicsFamilyHasValue = false;
    bool presentFamilyHasValue = false;
    [[nodiscard]] bool isComplete() const { return graphicsFamilyHasValue && presentFamilyHasValue; }
};

class Device {
public:
    constexpr bool enableValidationLayers = []() {
#ifdef NDEBUG
            return false;
#else
        return true;
#endif
    }();

    explicit Device(Window& window);
    ~Device();

    // Not copyable or movable
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] VkCommandPool getCommandPool() const { return commandPool; }
    [[nodiscard]] VkDevice device() const { return logicalDevice; }
    [[nodiscard]] VkSurfaceKHR surface() const { return windowSurface; }
    [[nodiscard]] VkQueue getGraphicsQueue() const { return graphicsQueue; }
    [[nodiscard]] VkQueue getPresentQueue() const { return presentQueue; }

    [[nodiscard]] SwapchainSupportDetails getSwapchainSupport() const { return querySwapchainSupport(physicalDevice); }
    [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] QueueFamilyIndices findPhysicalQueueFamilies() const { return findQueueFamilies(physicalDevice); }
    [[nodiscard]] VkFormat findSupportedFormat(
        const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    [[nodiscard]] VkSampleCountFlagBits evaluateMaxUsableSampleCount() const;

    // Buffer Helper Functions
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& buffer_memory) const;

    [[nodiscard]] VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer command_buffer) const;
    void copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const;
    void copyBufferToImage(
        VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layer_count) const;

    void createImageWithInfo(const VkImageCreateInfo& image_info, VkMemoryPropertyFlags properties, VkImage& image,
        VkDeviceMemory& image_memory) const;

    VkPhysicalDeviceProperties physicalProperties;
    unique_ptr<Shader> vertShader;
    unique_ptr<Shader> fragShader;

private:
    /**
 * \throws cth::except::data_exception data: VkResult vkCreateInstance()
 * \throws cth::except::default_exception reason: missing required validation layers
 */
    void createInstance();
    /**
     * \throws cth::except::data_exception data: VkResult of vkCreateDebugUtilsMessengerExt()
     */
    void setupDebugMessenger();

    void createSurface();

    /**
     * \throws cth::except::default_exception
     */
    void pickPhysicalDevice();
    /**
* \throws cth::except::data_exception data: VkResult vkCreateDeviceResult
* \throws cth::except::default_exception reason: missing required device features
*/
    void createLogicalDevice();
    void createCommandPool();
    void initShaders();


    string isDeviceSuitable(VkPhysicalDevice device) const;
    [[nodiscard]] vector<const char*> getRequiredExtensions() const;

    [[nodiscard]] string checkDeviceFeatures(const VkPhysicalDevice& device) const;

    /**
     *\throws cth::except::default_exception
     */
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo();

    [[nodiscard]] string checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;


    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    Window& window;
    VkCommandPool commandPool;

    VkDevice logicalDevice;
    VkSurfaceKHR windowSurface;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    constexpr array<const char*, 2> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};
    constexpr VkPhysicalDeviceFeatures requiredDeviceFeatures = []() {
        VkPhysicalDeviceFeatures features{};
        features.samplerAnisotropy = true;
        return features;
    }();
};

namespace dev {
    inline constexpr array<const char*, 1> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};
    [[nodiscard]] vector<const char*> getGLFWInstanceExtensions();

    [[nodiscard]] vector<const char*> getAvailableInstanceExtensions();

    /**
 * \brief checks for requested validation layers
 * \return unsupported layers
 */
    [[nodiscard]] string checkValidationLayers();
    [[nodiscard]] string hasGLFWRequiredInstanceExtensions();

    [[nodiscard]] string checkInstanceExtensionSupport();

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data);

    VkResult createDebugUtilsMessengerExt(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* _create_info,
        const VkAllocationCallbacks* _allocator, VkDebugUtilsMessengerEXT* _debug_messenger);

    void destroyDebugUtilsMessengerExt(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
        const VkAllocationCallbacks* _allocator);
}

}
//TEMP left off here, clean this up and make use of the Instance class continue refactoring afterward

