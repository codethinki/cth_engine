#pragma once
#include "core_config.hpp"
#include "device_table.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <initializer_list>
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
     */
    explicit Core(Instance const&);

    /**
     * constructs and creates
     * @details calls @ref Core(Instance const&), `create()`
     */
    Core(Instance const&, std::span<cth::ref_wrapper<Surface const> const> surfaces, Config const&);

    /**
     * constructs and creates with the surfaces of a braced list
     * @see @ref Core(Instance const&, std::span<cth::ref_wrapper<Surface const> const>, Config const&)
     */
    Core(
        Instance const& instance,
        std::initializer_list<cth::ref_wrapper<Surface const>> surfaces,
        Config const& config
    ) : Core{instance, std::span{surfaces}, config} {}

    /**
     * constructs and wraps
     * @details calls @ref Core(Instance const&), `wrap()`
     */
    Core(Instance const&, State);

    /**
     * @note calls `optDestroy()`
     */
    ~Core();

    /**
     * wraps the state
     * @details empty `State::queueIndices` -> entry i uses queue i of the device
     * @details `nullptr` `State::allocator` -> creates the allocator
     * @pre `Instance::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State);

    /**
     * creates the device, the allocator and the queues on a picked physical device
     * @param surfaces the present queues present to every surface
     * @details picks the first queue set that a physical device supports
     * @details picks the best physical device for it
     * @pre `Instance::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     * @throws cvk::cvk_exception reason: `Config::device` is missing or does not meet the requirements
     * @throws cvk::vk_result_exception result of `vkCreateDevice()`
     * @throws cvk::vk_result_exception result of `vmaCreateAllocator()`
     * @see `pick_physical_device()`, `Config::device`
     */
    void create(std::span<cth::ref_wrapper<Surface const> const> surfaces, Config const&);

    /**
     * creates with the surfaces of a braced list
     * @see @ref create(std::span<cth::ref_wrapper<Surface const> const>, Config const&)
     */
    void create(
        std::initializer_list<cth::ref_wrapper<Surface const>> surfaces,
        Config const& config
    ) { create(std::span{surfaces}, config); }

    /**
     * destroys and forgets
     * @pre `created()`
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    [[nodiscard]] constexpr bool created() const { return _devicePtr != nullptr; }

    /**
     * @return index of the used queue set in `Config::queueSets`
     * @pre `created()`
     * @pre the config had queue sets
     */
    [[nodiscard]] size_t queueSetIndex() const;

    [[nodiscard]] constexpr Instance const& instance() const { return *_instance; }
    [[nodiscard]] VkInstance vkInstance() const;

    /**
     * @pre `created()`
     */
    [[nodiscard]] PhysicalDevice const& physicalDevice() const;
    [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const;

    /**
     * @pre `created()`
     */
    [[nodiscard]] Device const& device() const;
    [[nodiscard]] DeviceTable deviceTable() const;
    [[nodiscard]] VolkDeviceTable const* functions() const;
    [[nodiscard]] VkDevice vkDevice() const;

    /**
     * @pre `created()`
     */
    [[nodiscard]] Allocator const& allocator() const;

    /**
     * @param queueIdx index of the queue in `Config::queueProperties`
     * @return the queue of the device, indices with the same `queueIndex()` return the same queue
     * @pre `created()`
     */
    [[nodiscard]] Queue const& queue(size_t queueIdx) const;

    /**
     * @param queueIdx index of the queue in `Config::queueProperties`
     * @return index of the queue in `Device::queues()`, indices with the same result share the `VkQueue`
     * @pre `created()`
     */
    [[nodiscard]] constexpr size_t queueIndex(size_t queueIdx) const {
        debug_check(*this);
        return _queueIndices[queueIdx];
    }

    /**
     * @return `queueIndex()` per entry of `Config::queueProperties`
     */
    [[nodiscard]] constexpr std::span<size_t const> queueIndices() const { return _queueIndices; }

private:
    struct device_requirements {
        DeviceFeatures features;
        std::vector<std::string> extensions;
    };

    struct device_pick {
        std::unique_ptr<PhysicalDevice> physicalDevice;
        std::vector<queue_family_index_t> uniqueQueueFamilyIndices;
        std::vector<size_t> queuesToUniqueQueues;

        /** `std::nullopt` -> the config had no queue sets */
        std::optional<size_t> queueSetIndex = std::nullopt;
    };
    struct queue_set_relations {
        std::vector<QueueFamilyProperties> uniqueQueues;
        std::vector<size_t> queueToUniqueQueue;
    };

    void forget();

    [[nodiscard]] static device_requirements requirements(Config const&);

    [[nodiscard]] device_pick pickPhysicalDevice(
        std::span<cth::ref_wrapper<Surface const> const> surfaces,
        Config const&,
        device_requirements const&
    ) const;

    [[nodiscard]] static queue_set_relations createQueueSetRelations(
        queue_set_t const& queueSet,
        std::span<QueueFamilyProperties const> queueProperties
    );

    [[nodiscard]] std::optional<device_pick> tryPickPhysicalDevice(
        std::span<cth::ref_wrapper<Surface const> const> surfaces,
        Config const&,
        device_requirements const&,
        queue_set_t const& queueSet
    ) const;

    cth::ref_wrapper<Instance const> _instance;

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
     * @pre `PhysicalDevice::created()`
     */
    unique_not_null<PhysicalDevice> physicalDevice;
    /**
     * @pre `Device::created()`
     * @pre created with `physicalDevice`
     */
    unique_not_null<Device> device;
    /**
     * @details `nullptr` -> `wrap()` creates it
     * @pre `Allocator::created()`
     * @pre created with `device`
     */
    std::unique_ptr<Allocator> allocator;
    /**
     * index in `Device::queues()` per entry of `Config::queueProperties`
     * @details empty -> entry i uses queue i of `device`
     * @pre every index < queue count of `device`
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
