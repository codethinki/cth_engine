#pragma once
#include "device_config.hpp"
#include "device_table.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/handle/unique_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <memory>
#include <span>
#include <string>
#include <vector>


namespace cvk {
class Instance;
class PhysicalDevice;

/**
 * VkDevice wrapper
 * @details owns the function table and the queues
 * @details the queues stay valid when the device moves
 */
class Device {
public:
    using Config = DeviceConfig;
    struct State;

    /**
     * base constructor
     * @pre @ref Instance::created()
     * @pre @ref PhysicalDevice::created()
     */
    Device(Instance const&, PhysicalDevice const&);

    /**
     * constructs and wraps
     * @details calls @ref Device(Instance const&, PhysicalDevice const&), @ref wrap()
     */
    Device(Instance const&, PhysicalDevice const&, State);

    /**
     * constructs and creates
     * @details calls @ref Device(Instance const&, PhysicalDevice const&), @ref create()
     */
    Device(Instance const&, PhysicalDevice const&, Config const&);

    /**
     * @note calls @ref optDestroy()
     */
    ~Device() { optDestroy(); }

    /**
     * wraps the state and retrieves the queues of @ref State::queueFamilyIndices
     * @details nullptr @ref State::functionTable -> loads the functions
     * @pre the device has the queues of @ref State::queueFamilyIndices
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State);

    /**
     * creates the device with the features and extensions of the config and retrieves the queues
     * @details queues of one family get the indices 0, 1, ... in the order of @ref Config::queueFamilyIndices
     * @pre the physical device has the features and the extensions of the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateDevice()
     */
    void create(Config const&);

    /**
     * destroys and forgets
     * @pre @ref created()
     */
    void destroy();

    /**
     * if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * blocks until every queue of the device finished its work
     * @pre @ref created()
     * @pre no other thread uses a queue of the device
     * @throws cvk::vk_result_exception result of vkDeviceWaitIdle()
     */
    void waitIdle() const;

    [[nodiscard]] constexpr std::span<Queue const> queues() const { return _queues; }
    [[nodiscard]] constexpr std::span<Queue> queues() { return _queues; }

    [[nodiscard]] constexpr DeviceTable table() const { return DeviceTable{_handle.get(), _functionTable.get()}; }
    [[nodiscard]] constexpr VolkDeviceTable const* functions() const { return _functionTable.get(); }
    [[nodiscard]] constexpr VkDevice get() const { return _handle.get(); }

    [[nodiscard]] constexpr Instance const& instance() const { return *_instance; }
    [[nodiscard]] constexpr PhysicalDevice const& physicalDevice() const { return *_physicalDevice; }

    /**
     * @return the enabled features
     */
    [[nodiscard]] constexpr DeviceFeatures const& features() const { return _features; }

    /**
     * @return the enabled extensions, the extensions of the features included
     */
    [[nodiscard]] constexpr std::span<std::string const> extensions() const { return _extensions; }

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }

private:
    void forget();

    /**
     * @param familyIndices family index of each queue
     * @return count of queues per family, one entry per family of the physical device
     * @pre the physical device has a family for each index
     */
    [[nodiscard]] std::vector<uint32_t> calcQueueFamiliesQueueCounts(std::span<uint32_t const> familyIndices) const;

    /**
     * creates the device with @ref _features and @ref _extensions
     * @param queueFamiliesQueueCounts count of queues per family, the index is the family index
     * @pre some count > 0
     * @throws cvk::vk_result_exception result of vkCreateDevice()
     */
    void createLogicalDevice(std::span<uint32_t const> queueFamiliesQueueCounts);

    void loadFunctionTable(VkDevice vkDevice) const;

    /**
     * retrieves the queues
     * @param familyIndices family index of each queue
     * @pre the device has the queues
     */
    void createQueues(std::span<queue_family_index_t const> familyIndices);


    not_null_ptr<Instance const> _instance;
    not_null_ptr<PhysicalDevice const> _physicalDevice;

    unique_handle<VkDevice, handle_deleter> _handle;
    std::unique_ptr<VolkDeviceTable> _functionTable = std::make_unique<VolkDeviceTable>();

    DeviceFeatures _features{};
    std::vector<std::string> _extensions{};
    std::vector<Queue> _queues{};

public:
    Device(Device const& other) = delete;
    Device(Device&& other) noexcept = default;
    Device& operator=(Device const& other) = delete;
    Device& operator=(Device&& other) noexcept = default;

    static constexpr void debug_check(Device const&);
    static constexpr void debug_check_handle(vk_not_null<VkDevice>);
};
}

namespace cvk {
struct Device::State {
    vk_not_null<VkDevice> vkDevice;
    /**
     * volk function table of @ref vkDevice
     * @details nullptr -> @ref wrap() loads it
     */
    std::unique_ptr<VolkDeviceTable> functionTable;

    /**
     * family index of each queue, see @ref Config::queueFamilyIndices
     */
    std::vector<queue_family_index_t> queueFamilyIndices{};

    /**
     * the features that the device enables
     */
    DeviceFeatures features{};

    /**
     * the extensions that the device enables, the extensions of the features included
     */
    std::vector<std::string> extensions{};
};
}

namespace cvk {
constexpr void Device::debug_check(Device const& device) {
    CTH_CRITICAL(!device.created(), "device must be created") {}
    debug_check_handle(device.get());
}

constexpr void Device::debug_check_handle([[maybe_unused]] vk_not_null<VkDevice> vkDevice) {}
}
