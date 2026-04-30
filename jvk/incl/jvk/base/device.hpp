#pragma once
#include "device_config.hpp"
#include "device_table.hpp"

#include "jvk/utility/types.hpp"

#include "queue/queue_family.hpp"

#include <volk.h>

#include <span>
#include <vector>



namespace jvk {
class Core;
class Surface;
class Instance;
class PhysicalDevice;
class Queue;


class Device {
    using queue_families_queue_counts_t = std::unordered_map<uint32_t, uint32_t>;

public:
    using Config = DeviceConfig;
    struct State;


    /**
     * @brief base constructor
     * @pre
     *  - @ref Instance::created()
     *  - @ref PhysicalDevice::created()
     */
    Device(Instance const&, PhysicalDevice const&);

    /**
     * @brief constructs and wraps
     * @details calls 
     *  - @ref Device(Instance const&, PhysicalDevice const&)
     *  - @ref wrap(State)
     */
    Device(Instance const&, PhysicalDevice const&, State);

    /**
     * @brief constructs and creates
     * @details calls:
     *  - @ref Device(Instance const&, PhysicalDevice const&)
     *  - @ref create(queue_property_view)
     */
    Device(Instance const&, PhysicalDevice const&, Config const&);

    /**
     * @details calls @ref optDestroy()
     */
    ~Device();

    /**
     * @brief wraps @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State state);

    /**
     * @brief creates device and queues
     * @details calls @ref optDestroy()
     */
    void create(Config const&);

    /**
     * @brief destroys and resets
     * @pre @ref created() 
     * @details calls @ref destroy(VkDevice)
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @pre @ref created()
     */
    State release();


    /**
     * @brief blocks until all vkQueueSubmit operations finished
     * @pre @ref created()
     */
    void waitIdle() const;

    /**
     * @brief destroys the device
     * @param vk_device should not be VK_NULL_HANDLE
     * @pre @ref destroy != `nullptr`
     */
    static void destroy(VkDevice vk_device, PFN_vkDestroyDevice destroy);

private:
    void reset();

    [[nodiscard]] static queue_families_queue_counts_t calcQueueFamiliesQueueCounts(
        std::span<uint32_t const> family_indices
    );

    /**
    * @throws jvk::result_exception result of @ref vkCreateDevice()
    */
    void createLogicalDevice(queue_families_queue_counts_t const&);

    void loadFunctionTable() const;

    /**
     * @brief retrieves the queues from the device
     * @param family_indices family index of each queue
     * @details calls @ref Queue::wrap(Queue::State const&)
     */
    void createQueues(std::span<unsigned const> family_indices);


    not_null<Instance const*> _instance;
    not_null<PhysicalDevice const*> _physicalDevice;

    move_ptr<VkDevice_T> _handle = VK_NULL_HANDLE;
    std::unique_ptr<VolkDeviceTable> _functionTable = std::make_unique<VolkDeviceTable>();

    std::vector<Queue> _queues{};

public:
    [[nodiscard]] std::span<Queue const> queues() const { return _queues; }
    [[nodiscard]] std::span<Queue> queues() { return _queues; }

    [[nodiscard]] DeviceTable table() const { return DeviceTable{_handle.get(), _functionTable.get()}; }
    [[nodiscard]] VolkDeviceTable const* functions() const { return _functionTable.get(); }
    [[nodiscard]] VkDevice get() const { return _handle.get(); }

    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }

    Device(Device const& other) = delete;
    Device(Device&& other) noexcept = default;
    Device& operator=(Device const& other) = delete;
    Device& operator=(Device&& other) noexcept = default;

    static void debug_check(Device const& device);
    static void debug_check_handle(jvk::vk_not_null<VkDevice> vk_device);
};
} // namespace cth

namespace jvk {
struct Device::State {
    jvk::vk_not_null<VkDevice> vkDevice;
    /**
     * @brief volk function table of @ref vkDevice
     * @attention must be loaded if not nullptr
     * @note may be nullptr
     */
    std::unique_ptr<VolkDeviceTable> functionTable;
};
}

//debug checks

namespace jvk {
inline void Device::debug_check(Device const& device) {
    CTH_CRITICAL(!device.created(), "device must be created") {}
    debug_check_handle(device.get());
}

inline void Device::debug_check_handle([[maybe_unused]] jvk::vk_not_null<VkDevice> vk_device) {}
}
