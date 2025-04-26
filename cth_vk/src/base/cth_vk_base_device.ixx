module;
#include "lib/volk.hpp"
#include <cth/io/io_log.hpp>

export module cth.vk.base.device;

import cth.vk.base.device_table;
import cth.vk.constants;
import cth.vk.util.types;

import cth.vk.base.instance;
import cth.vk.base.physical_device;
import cth.vk.submit.queue_info;
import cth.vk.submit.queue_family;


import cth.ptr;
import cth.io.log;

import std;


export namespace cth::vk {
class Device {
public:
    struct State;

    /**
     * @brief base constructor
     * @param instance @ref Instance::created() required
     * @param physical_device @ref PhysicalDevice::created() required
     */
    Device(Instance const& instance, PhysicalDevice const& physical_device);

    /**
     * @brief constructs and wraps
     * @note calls @ref Device(Instance const&, PhysicalDevice const&)
     * @note calls @ref wrap(State)
     */
    Device(Instance const& instance, PhysicalDevice const& physical_device, State state);

    /**
     * @brief constructs and creates
     * @note calls @ref Device(Instance const&, PhysicalDevice const&)
     * @note calls @ref create(std::span<Queue>)
     */
    Device(Instance const& instance, PhysicalDevice const& physical_device, std::span<QueueFamilyProperties const> queues);

    /**
     * @note calls @ref optDestroy()
     */
    ~Device();

    /**
     * @brief wraps @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State state);

    /**
     * @brief creates device and queues
     * @param[in, out] queues calls @ref Queue::wrap(VkQueue, uint32_t)
     * @note calls @ref optDestroy()
     */
    void create(std::span<QueueFamilyProperties const> queues);

    /**
     * @brief destroys and resets
     * @attention @ref created() required
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
     * @attention requires @ref created()
     */
    void waitIdle() const;

    /**
     * @brief destroys the device
     * @param vk_device should not be VK_NULL_HANDLE
     * @param destroy_function
     */
    static void destroy(VkDevice vk_device, PFN_vkDestroyDevice destroy_function);

private:
    void reset();

    /**
     * @brief sets the unique family indices
     * @return the queue family indices
     */
    [[nodiscard]] std::vector<uint32_t> setUniqueFamilyIndices(std::span<QueueFamilyProperties const> queues);
    /**
    * @throws cth::vk::result_exception result of @ref vkCreateDevice()
    */
    void createLogicalDevice();

    void loadFunctionTable() const;

    /**
     * @brief retrieves the queues from the device
     * @param family_indices family index of each queue
     * @note calls @ref Queue::wrap(Queue::State const&)
     */
    void wrapQueues(std::span<uint32_t const> family_indices, std::span<QueueFamilyProperties const> queues);


    cth::not_null<Instance const*> _instance;
    cth::not_null<PhysicalDevice const*> _physicalDevice;

    move_ptr<VkDevice_T> _handle = VK_NULL_HANDLE;
    std::unique_ptr<VolkDeviceTable> _functionTable = std::make_unique<VolkDeviceTable>();

    std::unordered_map<uint32_t, uint32_t> _queueFamiliesQueueCounts;
    std::vector<QueueInfo> _queueInfos;

public:
    [[nodiscard]] DeviceTable table() const { return DeviceTable{_handle.get(), _functionTable.get()}; }
    [[nodiscard]] VolkDeviceTable const* functions() const { return _functionTable.get(); }
    [[nodiscard]] VkDevice get() const { return _handle.get(); }
    [[nodiscard]] auto queueFamiliesQueueCounts() const { return _queueFamiliesQueueCounts; }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] std::span<QueueInfo const> queueInfos() { return _queueInfos; }

    Device(Device const& other) = delete;
    Device(Device&& other) noexcept = default;
    Device& operator=(Device const& other) = delete;
    Device& operator=(Device&& other) noexcept = default;

    static void debug_check(Device const& device);
    static void debug_check_handle(vk::not_null<VkDevice> vk_device);
};
} // namespace cth

export namespace cth::vk {
struct Device::State {
    vk::not_null<VkDevice> vkDevice;
    std::unordered_map<uint32_t, uint32_t> queueFamiliesQueueCounts;
    /**
     * @brief volk function table of @ref vkDevice
     * @attention must be loaded if not nullptr
     * @note may be nullptr
     */
    std::unique_ptr<VolkDeviceTable> functionTable;
};
}

//debug checks

export namespace cth::vk {
inline void Device::debug_check(Device const& device) {
    CTH_CRITICAL(!device.created(), "device must be created") {}
    debug_check_handle(device.get());
}
inline void Device::debug_check_handle([[maybe_unused]] vk::not_null<VkDevice> vk_device) {}
}
