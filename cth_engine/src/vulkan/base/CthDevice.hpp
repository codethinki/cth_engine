#pragma once
#include <memory>
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


private:
    void setQueueIndices(const Surface* surface);
    /**
    * \throws cth::except::vk_result_exception result of vkCreateDevice()
    */
    void createLogicalDevice();
    void setQueues();


    Instance* _instance;
    PhysicalDevice* _physicalDevice;

    VkCommandPool _commandPool = VK_NULL_HANDLE;

    VkDevice _vkDevice = VK_NULL_HANDLE;
    VkQueue _vkGraphicsQueue = VK_NULL_HANDLE;
    VkQueue _vkPresentQueue = VK_NULL_HANDLE;

    //TODO replace this with a better system 
    vector<uint32_t> _queueIndices; //present, graphics
    vector<uint32_t> _uniqueQueueIndices{};
    static constexpr uint32_t PRESENT_QUEUE_I = 0;
    static constexpr uint32_t GRAPHICS_QUEUE_I = 1;

public:
    [[nodiscard]] VkCommandPool getCommandPool() const { return _commandPool; }
    [[nodiscard]] VkDevice get() const { return _vkDevice; }
    [[nodiscard]] VkQueue graphicsQueue() const { return _vkGraphicsQueue; }
    [[nodiscard]] VkQueue presentQueue() const { return _vkPresentQueue; }
    [[nodiscard]] uint32_t graphicsQueueIndex() const { return _queueIndices[GRAPHICS_QUEUE_I]; }
    [[nodiscard]] uint32_t presentQueueIndex() const { return _queueIndices[PRESENT_QUEUE_I]; }
    [[nodiscard]] const auto& queueIndices() const { return _queueIndices; }
    [[nodiscard]] const auto& uniqueQueueIndices() const { return _uniqueQueueIndices; }

    [[nodiscard]] const PhysicalDevice* physical() const { return _physicalDevice; }

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = default;
    Device& operator=(Device&&) = default;

#ifdef _DEBUG
#define DEBUG_CHECK_DEVICE(device_ptr) Device::debug_check(device_ptr)
    static void debug_check(const Device* device);
#else
#define DEBUG_CHECK_DEVICE(device_ptr) ((void)0)
#endif
};
} // namespace cth
