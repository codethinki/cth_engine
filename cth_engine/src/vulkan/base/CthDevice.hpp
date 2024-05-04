#pragma once
#include "vulkan/utility/CthConstants.hpp"

#include <cth/cth_memory.hpp>

#include <vulkan/vulkan.h>

#include <vector>


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


    mem::basic_ptr<VkDevice_T> _handle = VK_NULL_HANDLE;
    VkQueue _vkGraphicsQueue = VK_NULL_HANDLE;
    VkQueue _vkPresentQueue = VK_NULL_HANDLE;

    //TODO replace this with a better system 
    vector<uint32_t> _queueIndices; //present, graphics
    vector<uint32_t> _uniqueQueueIndices{};
    static constexpr uint32_t PRESENT_QUEUE_I = 0;
    static constexpr uint32_t GRAPHICS_QUEUE_I = 1;

public:
    [[nodiscard]] VkDevice get() const { return _handle.get(); }
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

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const Device* device);
#define DEBUG_CHECK_DEVICE(device_ptr) Device::debug_check(device_ptr)
#else
#define DEBUG_CHECK_DEVICE(device_ptr) ((void)0)
#endif
};
} // namespace cth
