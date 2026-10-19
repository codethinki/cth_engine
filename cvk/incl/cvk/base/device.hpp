#pragma once
#include "device_config.hpp"
#include "device_table.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/utility/handle/unique_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>


namespace cvk {
class Instance;
class PhysicalDevice;

/**
 * @brief VkDevice wrapper, owns the function table and the queues
 */
class Device {
    using queue_families_queue_counts_t = std::unordered_map<uint32_t, uint32_t>;

public:
    using vk_handle_t = VkDevice;
    using Config = DeviceConfig;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Instance::created()
     * @pre @ref PhysicalDevice::created()
     */
    Device(Instance const&, PhysicalDevice const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Device(Instance const&, PhysicalDevice const&)
     * - @ref wrap()
     */
    Device(Instance const&, PhysicalDevice const&, State);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Device(Instance const&, PhysicalDevice const&)
     * - @ref create()
     */
    Device(Instance const&, PhysicalDevice const&, Config const&);

    /**
     * @note calls @ref optDestroy()
     */
    ~Device() { optDestroy(); }

    /**
     * @brief wraps the state and retrieves the queues of @ref State::queueFamilyIndices
     * @details nullptr @ref State::functionTable -> loads the functions
     * @pre the device has the queues of @ref State::queueFamilyIndices
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State);

    /**
     * @brief creates the device with the features and extensions of the config and retrieves the queues
     * @pre the physical device has the features and the extensions of the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateDevice()
     */
    void create(Config const&);

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @brief blocks until every queue of the device finished its work
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkDeviceWaitIdle()
     */
    void waitIdle() const;

    /**
     * @brief destroys the device, gets vkDestroyDevice through vkGetDeviceProcAddr()
     */
    static void destroy(VkDevice);

    [[nodiscard]] constexpr std::span<Queue const> queues() const { return _queues; }
    [[nodiscard]] constexpr std::span<Queue> queues() { return _queues; }

    [[nodiscard]] constexpr DeviceTable table() const { return DeviceTable{_handle.get(), _functionTable.get()}; }
    [[nodiscard]] constexpr VolkDeviceTable const* functions() const { return _functionTable.get(); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }

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

    [[nodiscard]] static queue_families_queue_counts_t calcQueueFamiliesQueueCounts(
        std::span<uint32_t const> familyIndices
    );

    /**
     * @brief creates the device with @ref _features and @ref _extensions
     * @throws cvk::vk_result_exception result of vkCreateDevice()
     */
    void createLogicalDevice(queue_families_queue_counts_t const& queueFamiliesQueueCounts);

    void loadFunctionTable(VkDevice vkDevice) const;

    /**
     * @brief retrieves the queues
     * @param familyIndices family index of each queue
     * @pre the device has the queues
     */
    void createQueues(std::span<queue_family_index_t const> familyIndices);


    not_null_ptr<Instance const> _instance;
    not_null_ptr<PhysicalDevice const> _physicalDevice;

    unique_handle<vk_handle_t, handle_deleter<Device>> _handle;
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
     * @brief volk function table of @ref vkDevice
     * @details nullptr -> @ref wrap() loads it
     */
    std::unique_ptr<VolkDeviceTable> functionTable;

    /**
     * @brief family index of each queue, see @ref Config::queueFamilyIndices
     */
    std::vector<queue_family_index_t> queueFamilyIndices{};

    /**
     * @brief the features that the device enables
     */
    DeviceFeatures features{};

    /**
     * @brief the extensions that the device enables, the extensions of the features included
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
