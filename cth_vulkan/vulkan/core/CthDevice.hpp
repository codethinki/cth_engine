#pragma once
#include <array>

#include "../utils/HlcShader.hpp"

#include <memory>
#include <vector>

namespace cth {
inline const std::filesystem::path SHADER_COMPILER_PATH = L"E:/visual_studio/SDK/Vulkan/Bin/glslc.exe";
inline const std::filesystem::path VERTEX_SHADER_CODE_PATH = L"resources/shaders/vertShader.vert";
inline const std::filesystem::path FRAGMENT_SHADER_CODE_PATH = L"resources/shaders/fragShader.frag";
inline const std::filesystem::path VERTEX_SHADER_BINARIES_PATH = L"resources/shaders/vertShader.spv";
inline const std::filesystem::path FRAGMENT_SHADER_BINARIES_PATH = L"resources/shaders/fragShader.spv";

class Instance;

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
    [[nodiscard]] bool complete() const { return graphicsFamily() && presentFamily(); }

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

    explicit Device(Window* window);
    ~Device();

    [[nodiscard]] SwapchainSupportDetails getSwapchainSupport() const { return querySwapchainSupport(physicalDevice); }
    /**
     * \throws cth::except::default_exception reason: no suitable memory type
     */
    [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] QueueFamilyIndices findPhysicalQueueFamilies() const { return findQueueFamilies(physicalDevice); }
    /**
     *\throws cth::except::data_exception data: features param
     */
    [[nodiscard]] VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    [[nodiscard]] VkSampleCountFlagBits evaluateMaxUsableSampleCount() const;

    // Buffer Helper Functions

    /**
     * \throws cth::except::data_exception data: VkResult of vkCreateBuffer()
     * \throws cth::except::data_exception data: VkResult of vkAllocateMemory()
     */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& buffer_memory) const;

    [[nodiscard]] VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer command_buffer) const;
    void copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layer_count) const;

    //TODO put this into the image class maybe
    /**
     * \throws cth::except::data_exception data: VkResult of vkCreateImage()
     * \throws cth::except::data_exception data: VkResult of vkAllocateMemory()
     * \throws cth::except::data_exception data: VkResult of vkBindImageMemory()
     */
    void createImageWithInfo(const VkImageCreateInfo& image_info, VkMemoryPropertyFlags properties, VkImage& image,
        VkDeviceMemory& image_memory) const;


    VkPhysicalDeviceProperties physicalProperties;
    unique_ptr<Shader> vertShader;
    unique_ptr<Shader> fragShader;

private:
    //createInstance
    static [[nodiscard]] vector<const char*> getGLFWInstanceExtensions();
    [[nodiscard]] vector<const char*> getRequiredInstanceExtensions() const;
    /**
 * \throws cth::except::data_exception data: VkResult vkCreateInstance()
 * \throws cth::except::default_exception reason: missing required instance extensions
 * \throws cth::except::default_exception reason: missing required validation layers
 */
    void createInstance();
    //createSurface
    void createSurface();

    //pickPhysicalDevice
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;
    [[nodiscard]] vector<const char*> checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    [[nodiscard]] vector<uint32_t> checkDeviceFeatureSupport(const VkPhysicalDevice& device) const;
    [[nodiscard]] bool physicalDeviceSuitable(VkPhysicalDevice device) const;
    /**
     * \throws cth::except::default_exception reason: no vulkan gpus
     * \throws cth::except::default_exception reason: no suitable gpu
     */
    void pickPhysicalDevice();
    //createLogicalDevice
    /**
* \throws cth::except::data_exception data: VkResult of vkCreateDevice()
*/
    void createLogicalDevice();
    //createCommandPool
    /**
     * \throws cth::except::data_exception data: VkResult of vkCreateCommandPool()
     */
    void createCommandPool();
    //initShaders
    void initShaders();


    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    Window* window;
    VkCommandPool commandPool;

    VkDevice logicalDevice;
    VkSurfaceKHR windowSurface;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    unique_ptr<Instance> instance;

public:
    // Not copyable or movable
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] VkCommandPool getCommandPool() const { return commandPool; }
    [[nodiscard]] VkDevice device() const { return logicalDevice; } //TODO rename this to get()
    [[nodiscard]] VkSurfaceKHR surface() const { return windowSurface; }
    [[nodiscard]] VkQueue getGraphicsQueue() const { return graphicsQueue; }
    [[nodiscard]] VkQueue getPresentQueue() const { return presentQueue; }
};
} // namespace cth
