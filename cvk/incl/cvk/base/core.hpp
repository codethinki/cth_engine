#pragma once
#include "core_config.hpp"
#include "device_table.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>


namespace cvk {
class Allocator;
class Device;
class Instance;
class PhysicalDevice;
class Surface;

/**
 * device core
 * @details picks the physical device
 * @details owns the device, the allocator and the queues
 * @details the queues stay valid when the core moves
 * @details the instance outlives the core
 */
class Core {
public:
    using Config = CoreConfig;
    using queue_set_t = Config::queue_set_t;
    struct State;

    /**
     * base constructor
     * @pre @ref Instance::created()
     */
    explicit Core(Instance const&);

    /**
     * constructs and creates
     * @details calls @ref Core(Instance const&), @ref create()
     */
    Core(Instance const&, std::span<Surface const> surfaces, Config const&);

    /**
     * constructs and wraps
     * @details calls @ref Core(Instance const&), @ref wrap()
     */
    Core(Instance const&, State);

    /**
     * @note calls @ref optDestroy()
     */
    ~Core();

    /**
     * wraps the state
     * @details empty @ref State::queueIndices -> entry i uses queue i of the device
     * @details nullptr @ref State::allocator -> creates the allocator
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State);

    /**
     * creates the device, the allocator and the queues on a picked physical device
     * @param surfaces the present queues present to every surface
     * @details picks the first queue set that a physical device supports
     * @details picks the best physical device for it, see @ref pick_physical_device() and @ref Config::device
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     * @throws cvk::cvk_exception reason: @ref Config::device is missing or does not meet the requirements
     * @throws cvk::vk_result_exception result of vkCreateDevice()
     * @throws cvk::vk_result_exception result of vmaCreateAllocator()
     */
    void create(std::span<Surface const> surfaces, Config const&);

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

    [[nodiscard]] constexpr bool created() const { return _devicePtr != nullptr; }

    /**
     * @return index of the used queue set in @ref Config::queueSets
     * @pre @ref created()
     * @pre the config had queue sets
     */
    [[nodiscard]] size_t queueSetIndex() const;

    [[nodiscard]] constexpr Instance const& instance() const { return *_instancePtr; }
    [[nodiscard]] VkInstance vkInstance() const;

    /**
     * @pre @ref created()
     */
    [[nodiscard]] PhysicalDevice const& physicalDevice() const;
    [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const;

    /**
     * @pre @ref created()
     */
    [[nodiscard]] Device const& device() const;
    [[nodiscard]] DeviceTable deviceTable() const;
    [[nodiscard]] VolkDeviceTable const* functions() const;
    [[nodiscard]] VkDevice vkDevice() const;

    /**
     * @pre @ref created()
     */
    [[nodiscard]] Allocator const& allocator() const;

    /**
     * @param queueIdx index of the queue in @ref Config::queueProperties
     * @return the queue of the device, indices with the same @ref queueIndex() return the same queue
     * @pre @ref created()
     */
    [[nodiscard]] Queue const& queue(size_t queueIdx) const;

    /**
     * @param queueIdx index of the queue in @ref Config::queueProperties
     * @return index of the queue in @ref Device::queues(), indices with the same result share the VkQueue
     * @pre @ref created()
     */
    [[nodiscard]] constexpr size_t queueIndex(size_t queueIdx) const {
        debug_check(*this);
        return _queueIndices[queueIdx];
    }

    /**
     * @return @ref queueIndex() per entry of @ref Config::queueProperties
     */
    [[nodiscard]] constexpr std::span<size_t const> queueIndices() const { return _queueIndices; }

private:
    /**
     * features and extensions of the device
     */
    struct device_requirements {
        DeviceFeatures features;
        std::vector<std::string> extensions;
    };

    /**
     * physical device for a queue set and the mapping of the queues
     */
    struct device_pick {
        std::unique_ptr<PhysicalDevice> physicalDevice;
        std::vector<queue_family_index_t> uniqueQueueFamilyIndices;
        std::vector<size_t> queuesToUniqueQueues;

        /**
         * std::nullopt -> the config had no queue sets
         */
        std::optional<size_t> queueSetIndex = std::nullopt;
    };
    struct queue_set_relations {
        std::vector<QueueFamilyProperties> uniqueQueues;
        std::vector<size_t> queueToUniqueQueue;
    };

    void forget();

    /**
     * @return the base set, the config and the present set with a present queue
     */
    [[nodiscard]] static device_requirements requirements(Config const&);

    /**
     * picks a physical device for the first possible queue set
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     * @throws cvk::cvk_exception reason: @ref Config::device is missing or does not meet the requirements
     */
    [[nodiscard]] device_pick pickPhysicalDevice(
        std::span<Surface const> surfaces,
        Config const&,
        device_requirements const&
    ) const;

    [[nodiscard]] static queue_set_relations createQueueSetRelations(
        queue_set_t const& queueSet,
        std::span<QueueFamilyProperties const> queueProperties
    );

    /**
     * @return std::nullopt -> no physical device supports the queue set
     */
    [[nodiscard]] std::optional<device_pick> tryPickPhysicalDevice(
        std::span<Surface const> surfaces,
        Config const&,
        device_requirements const&,
        queue_set_t const& queueSet
    ) const;

    not_null_ptr<Instance const> _instancePtr;

    std::unique_ptr<PhysicalDevice> _physicalDevicePtr;
    // declared before the device: move assignment destroys the old allocator before the old device
    std::unique_ptr<Allocator> _allocatorPtr;
    std::unique_ptr<Device> _devicePtr;
    std::optional<size_t> _queueSetIdxOpt;
    std::vector<size_t> _queueIndices;

public:
    Core(Core const& other) = delete;
    Core(Core&& other) noexcept;
    Core& operator=(Core const& other) = delete;
    Core& operator=(Core&& other) noexcept;

    static constexpr void debug_check(Core const&);
};
}

namespace cvk {
struct Core::State {
    /**
     * @pre @ref PhysicalDevice::created()
     */
    unique_not_null<PhysicalDevice> physicalDevice;
    /**
     * @pre @ref Device::created()
     * @pre created with @ref physicalDevice
     */
    unique_not_null<Device> device;
    /**
     * @details nullptr -> @ref wrap() creates it
     * @pre @ref Allocator::created()
     * @pre created with @ref device
     */
    std::unique_ptr<Allocator> allocator;
    /**
     * index in @ref Device::queues() per entry of @ref Config::queueProperties
     * @details empty -> entry i uses queue i of @ref device
     * @pre every index < queue count of @ref device
     */
    std::vector<size_t> queueIndices{};
    std::optional<size_t> queueSetIndex = std::nullopt;
};
}

namespace cvk {
constexpr void Core::debug_check(Core const& core) {
    CTH_CRITICAL(!core.created(), "core must be created") {}
}
}
