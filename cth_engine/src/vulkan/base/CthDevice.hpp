#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>


namespace cth {
using namespace std;
class Surface;
class Instance;
class PhysicalDevice;



class Device {
public:
    explicit Device(PhysicalDevice* physical_device, Surface* surface, Instance* instance);
    ~Device();

    /**
     * \throws cth::except::default_exception reason: no suitable memory type
     */
    [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
    /**
     *\throws cth::except::data_exception data: features param
     */
    [[nodiscard]] VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

    // Buffer Helper Functions

    /**
     * \throws cth::except::vk_result_exception result of vkCreateBuffer()
     * \throws cth::except::vk_result_exception result of vkAllocateMemory()
     */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& buffer_memory) const;

    [[nodiscard]] VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer command_buffer) const;

    //TODO put this into the image class maybe
    /**
     * \throws cth::except::vk_result_exception result of vkCreateImage()
     * \throws cth::except::vk_result_exception result of vkAllocateMemory()
     * \throws cth::except::vk_result_exception result of vkBindImageMemory()
     */
    void createImageWithInfo(const VkImageCreateInfo& image_info, VkMemoryPropertyFlags properties, VkImage& image,
        VkDeviceMemory& image_memory) const;

private:
    //createLogicalDevice
    /**
    * \throws cth::except::vk_result_exception result of vkCreateDevice()
    */
    void createLogicalDevice(const Surface* surface);

    //createCommandPool
    /**
     * \throws cth::except::vk_result_exception result of vkCreateCommandPool()
     */
    void createCommandPool();

    Instance* instance;
    PhysicalDevice* physicalDevice;

    VkCommandPool commandPool = VK_NULL_HANDLE;

    VkDevice vkDevice = VK_NULL_HANDLE;
    VkQueue vkGraphicsQueue = VK_NULL_HANDLE;
    VkQueue vkPresentQueue = VK_NULL_HANDLE;

    //TODO replace this with a better system 
    vector<uint32_t> _queueIndices; //graphics, present
    static constexpr uint32_t GRAPHICS_QUEUE_INDEX = 0;
    static constexpr uint32_t PRESENT_QUEUE_INDEX = 1;

    VkPhysicalDeviceProperties physicalProperties;

public:
    [[nodiscard]] VkCommandPool getCommandPool() const { return commandPool; }
    [[nodiscard]] VkDevice get() const { return vkDevice; }
    [[nodiscard]] VkQueue graphicsQueue() const { return vkGraphicsQueue; }
    [[nodiscard]] VkQueue presentQueue() const { return vkPresentQueue; }
    [[nodiscard]] uint32_t graphicsQueueIndex() const { return _queueIndices[GRAPHICS_QUEUE_INDEX]; }
    [[nodiscard]] uint32_t presentQueueIndex() const { return _queueIndices[PRESENT_QUEUE_INDEX]; }


    [[nodiscard]] const PhysicalDevice* physical() const { return physicalDevice; }

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;
};
} // namespace cth
