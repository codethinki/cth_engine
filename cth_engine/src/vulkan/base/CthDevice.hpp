#pragma once
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/utility/cth_vk_types.hpp"

#include<cth/pointers.hpp>

#include <vulkan/vulkan.h>


#include <span>
#include <vector>



namespace cth::vk {
class Core;
class Surface;
class Instance;
class PhysicalDevice;
class Queue;

//TEMP add create, wrap, constructors, release optDestroy, destroy 

class Device {
public:
    struct State;

    /**
     * @brief base constructor
     * @param instance @ref Instance::created() required
     * @param physical_device @ref PhysicalDevice::created() required
     */
    explicit Device(cth::not_null<Instance const*> instance, cth::not_null<PhysicalDevice const*> physical_device);

    /**
     * @brief constructs and wraps
     * @note calls @ref Device(cth::not_null<Instance const*>, cth::not_null<PhysicalDevice const*>)
     * @note calls @ref wrap(State)
     */
    explicit Device(cth::not_null<Instance const*> instance, cth::not_null<PhysicalDevice const*> physical_device, State const& state);

    /**
     * @brief constructs and creates
     * @note calls @ref Device(cth::not_null<Instance const*>, cth::not_null<PhysicalDevice const*>)
     * @note calls @ref create(std::span<Queue>)
     */
    explicit Device(cth::not_null<Instance const*> instance, cth::not_null<PhysicalDevice const*> physical_device, std::span<Queue> queues);
    /**
     * @note calls @ref optDestroy()
     */
    ~Device();

    /**
     * @brief wraps @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief creates device and queues
     * @param[in, out] queues calls @ref Queue::wrap(VkQueue, uint32_t)
     * @note calls @ref optDestroy()
     */
    void create(std::span<Queue> queues);

    /**
     * @brief destroys and resets
     * @attention requires @ref created()
     * @note calls @ref destroy(VkDevice)
     */
    void destroy();
    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @attention requires @ref created()
     */
    State release();


    /**
     * @brief blocks until all vkQueueSubmit operations finished
     */
    void waitIdle() const;

    /**
     * @brief destroys the device
     * @param vk_device should not be VK_NULL_HANDLE
     */
    static void destroy(VkDevice vk_device);

private:
    void reset();

    /**
     * @brief sets the unique family indices
     * @return the queue family indices
     */
    [[nodiscard]] std::vector<uint32_t> setUniqueFamilyIndices(std::span<Queue const> queues);
    /**
    * @throws cth::vk::result_exception result of @ref vkCreateDevice()
    */
    void createLogicalDevice();
    void wrapQueues(std::span<uint32_t const> family_indices, std::span<Queue> queues) const;


    cth::not_null<Instance const*> _instance;
    cth::not_null<PhysicalDevice const*> _physicalDevice;

    move_ptr<VkDevice_T> _handle = VK_NULL_HANDLE;

    //TODO replace this with a better system
    std::vector<uint32_t> _uniqueFamilyIndices; //present, graphicsPhase


public:
    [[nodiscard]] VkDevice get() const { return _handle.get(); }
    [[nodiscard]] auto familyIndices() const { return _uniqueFamilyIndices; }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }

    Device(Device const& other) = delete;
    Device(Device&& other) noexcept = default;
    Device& operator=(Device const& other) = delete;
    Device& operator=(Device&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(cth::not_null<Device const*> device);
    static void debug_check_handle(vk::not_null<VkDevice> vk_device);
#define DEBUG_CHECK_DEVICE(device_ptr) Device::debug_check(device_ptr)
#define DEBUG_CHECK_DEVICE_HANDLE(vk_device) Device::debug_check_handle(vk_device)
#else
#define DEBUG_CHECK_DEVICE(device_ptr) ((void)0)
#define DEBUG_CHECK_DEVICE_HANDLE(vk_device) ((void)0)
#endif
};
} // namespace cth

namespace cth::vk {
struct Device::State {
    vk::not_null<VkDevice> handle;
    std::vector<uint32_t> familyIndices;
};
}
