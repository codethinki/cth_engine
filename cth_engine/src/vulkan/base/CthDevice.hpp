#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include<cth/cth_pointer.hpp>

#include <vulkan/vulkan.h>


#include <span>
#include <vector>



namespace cth::vk {
class BasicCore;
class Surface;
class Instance;
class PhysicalDevice;
class Queue;


class Device {
public:
    explicit Device(Instance const* instance, PhysicalDevice const* physical_device, std::span<Queue> queues);
    ~Device();

    void waitIdle() const;
private:
    /**
     * @brief sets the unique family indices
     * @return the queue family indices
     */
    [[nodiscard]] std::vector<uint32_t> setUniqueFamilyIndices(std::span<Queue const> queues);
    /**
    * @throws cth::except::vk_result_exception result of vkCreateDevice()
    */
    void createLogicalDevice();
    void wrapQueues(std::span<uint32_t const> family_indices, std::span<Queue> queues) const;


    Instance const* _instance;
    PhysicalDevice const* _physicalDevice;

    move_ptr<VkDevice_T> _handle = VK_NULL_HANDLE;

    //TODO replace this with a better system
    std::vector<uint32_t> _familyIndices; //present, graphicsPhase


public:
    [[nodiscard]] VkDevice get() const { return _handle.get(); }
    [[nodiscard]] auto familyIndices() const { return _familyIndices; }

    Device(Device const&) = delete;
    Device& operator=(Device const&) = delete;
    Device(Device&&) = default;
    Device& operator=(Device&&) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(Device const* device);
    static void debug_check_handle(VkDevice vk_device);
#define DEBUG_CHECK_DEVICE(device_ptr) Device::debug_check(device_ptr)
#define DEBUG_CHECK_DEVICE_HANDLE(vk_device) Device::debug_check_handle(vk_device)
#else
#define DEBUG_CHECK_DEVICE(device_ptr) ((void)0)
#define DEBUG_CHECK_DEVICE_HANDLE(vk_device) ((void)0)
#endif
};
} // namespace cth
