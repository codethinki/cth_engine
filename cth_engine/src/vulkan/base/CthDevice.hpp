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
    explicit Device(PhysicalDevice* physical_device, const Surface* surface, Instance* instance);
    ~Device();


    [[nodiscard]] VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer command_buffer) const;

private:
    void setQueueIndices(const Surface* surface);
    /**
    * \throws cth::except::vk_result_exception result of vkCreateDevice()
    */
    void createLogicalDevice();
    void setQueues();


    Instance* instance;
    PhysicalDevice* physicalDevice;

    VkCommandPool commandPool = VK_NULL_HANDLE;

    VkDevice vkDevice = VK_NULL_HANDLE;
    VkQueue vkGraphicsQueue = VK_NULL_HANDLE;
    VkQueue vkPresentQueue = VK_NULL_HANDLE;

    //TODO replace this with a better system 
    vector<uint32_t> _queueIndices; //present, graphics
    vector<uint32_t> _uniqueQueueIndices{};
    static constexpr uint32_t PRESENT_QUEUE_I = 0;
    static constexpr uint32_t GRAPHICS_QUEUE_I = 1;

public:
    [[nodiscard]] VkCommandPool getCommandPool() const { return commandPool; }
    [[nodiscard]] VkDevice get() const { return vkDevice; }
    [[nodiscard]] VkQueue graphicsQueue() const { return vkGraphicsQueue; }
    [[nodiscard]] VkQueue presentQueue() const { return vkPresentQueue; }
    [[nodiscard]] uint32_t graphicsQueueIndex() const { return _queueIndices[GRAPHICS_QUEUE_I]; }
    [[nodiscard]] uint32_t presentQueueIndex() const { return _queueIndices[PRESENT_QUEUE_I]; }
    [[nodiscard]] const auto& queueIndices() const { return _queueIndices; }
    [[nodiscard]] const auto& uniqueQueueIndices() const { return _uniqueQueueIndices; }

    [[nodiscard]] const PhysicalDevice* physical() const { return physicalDevice; }

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

#ifdef _DEBUG
    static void debug_check(const Device* device);
#else
    static void debug_check(const Device* device) {}
#endif
};
} // namespace cth
