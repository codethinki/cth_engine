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
 * @brief picks the physical device and owns it with the device, the allocator and the queues
 * @details the instance outlives the core,
 *  the queues stay valid when the core moves
 */
class Core {
public:
    using Config = CoreConfig;
    using queue_set_t = Config::queue_set_t;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Instance::created()
     */
    explicit Core(Instance const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Core(Instance const&)
     * - @ref create()
     */
    Core(Instance const&, std::span<Surface const> surfaces, Config const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Core(Instance const&)
     * - @ref wrap()
     */
    Core(Instance const&, State);

    /**
     * @note calls @ref optDestroy()
     */
    ~Core();

    /**
     * @brief wraps the state
     * @details empty @ref State::queueIndices -> entry i uses queue i of the device,
     *  nullptr @ref State::allocator -> creates the allocator
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State);

    /**
     * @brief picks a physical device for the first possible queue set, creates the device, the allocator and the queues
     * @param surfaces the present queues present to every surface
     * @details the first queue set that a physical device supports wins,
     *  then the best physical device for this set, see @ref pick_physical_device() and @ref Config::device
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     * @throws cvk::cvk_exception reason: @ref Config::device is missing or does not meet the requirements
     * @throws cvk::vk_result_exception result of vkCreateDevice()
     * @throws cvk::vk_result_exception result of vmaCreateAllocator()
     */
    void create(std::span<Surface const> surfaces, Config const&);

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

    [[nodiscard]] constexpr bool created() const { return _device != nullptr; }

    /**
     * @return index of the used queue set in @ref Config::queueSets
     * @pre @ref created()
     * @pre the config had queue sets
     */
    [[nodiscard]] size_t queueSetIndex() const;

    [[nodiscard]] constexpr Instance const& instance() const { return *_instance; }
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
     * @param idx index in @ref Config::queueProperties
     * @return the queue of the device, entries with the same @ref queueIndex() return the same queue
     * @pre @ref created()
     */
    [[nodiscard]] Queue const& queue(size_t idx) const;

    /**
     * @param idx index in @ref Config::queueProperties
     * @return index of the queue in @ref Device::queues(), entries with the same index share the VkQueue
     * @pre @ref created()
     */
    [[nodiscard]] constexpr size_t queueIndex(size_t idx) const {
        debug_check(*this);
        return _queueIndices[idx];
    }

    /**
     * @return @ref queueIndex() per entry of @ref Config::queueProperties
     */
    [[nodiscard]] constexpr std::span<size_t const> queueIndices() const { return _queueIndices; }

private:
    /**
     * @brief features and extensions of the device
     */
    struct device_requirements {
        DeviceFeatures features;
        std::vector<std::string> extensions;
    };

    /**
     * @brief physical device for a queue set and the mapping of the queues
     */
    struct device_pick {
        std::unique_ptr<PhysicalDevice> physicalDevice;
        std::vector<queue_family_index_t> uniqueQueueFamilyIndices;
        std::vector<size_t> queuesToUniqueQueues;

        /**
         * @brief std::nullopt -> the config had no queue sets
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
     * @brief picks a physical device for the first possible queue set
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

    not_null_ptr<Instance const> _instance;

    std::unique_ptr<PhysicalDevice> _physicalDevice;
    // declared before the device: move assignment destroys the old allocator before the old device
    std::unique_ptr<Allocator> _allocator;
    std::unique_ptr<Device> _device;
    std::optional<size_t> _queueSetIndex;
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
     * @brief index in @ref Device::queues() per entry of @ref Config::queueProperties
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
