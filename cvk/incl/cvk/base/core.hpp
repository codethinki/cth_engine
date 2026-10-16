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
#include <vector>


namespace cvk {
class Device;
class Instance;
class PhysicalDevice;
class Surface;

/**
 * @brief picks the physical device and owns it with the device and the queues
 * @details the instance outlives the core
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
    explicit Core(Instance const& instance);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Core(Instance const&)
     * - @ref create()
     */
    Core(Instance const& instance, std::span<Surface const> surfaces, Config const& config);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Core(Instance const&)
     * - @ref wrap()
     */
    Core(Instance const& instance, State state);

    /**
     * @note calls @ref optDestroy()
     */
    ~Core();

    /**
     * @brief wraps the state
     * @details empty @ref State::queues -> copies the queues of the device
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State state);

    /**
     * @brief picks a physical device for the first possible queue set, creates the device and the queues
     * @param surfaces the present queues present to every surface
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     * @throws cvk::vk_result_exception result of vkCreateDevice()
     */
    void create(std::span<Surface const> surfaces, Config const& config);

    /**
     * @brief destroys and resets
     * @pre @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and resets
     * @pre @ref created()
     */
    State release();

    [[nodiscard]] bool created() const { return _device != nullptr; }

    /**
     * @return index of the used queue set in @ref Config::queueSets
     * @pre @ref created()
     * @pre the config had queue sets
     */
    [[nodiscard]] size_t queueSetIndex() const;

    [[nodiscard]] Instance const& instance() const { return *_instance; }
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
     * @param idx index in @ref Config::queueProperties
     * @pre @ref created()
     */
    [[nodiscard]] auto& queue(this auto& self, size_t idx) {
        debug_check(self);
        return self._queues[idx];
    }

    [[nodiscard]] std::span<Queue const> queues() const { return _queues; }

private:
    struct queue_mappings {
        std::vector<queue_family_index_t> uniqueQueueFamilyIndices;
        std::vector<size_t> queuesToUniqueQueues;
    };
    struct queue_set_relations {
        std::vector<QueueFamilyProperties> uniqueQueues;
        std::vector<size_t> queueToUniqueQueue;
    };

    void reset();

    /**
     * @post sets @ref _physicalDevice and @ref _queueSetIndex
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     */
    [[nodiscard]] queue_mappings createPhysicalDevice(std::span<Surface const> surfaces, Config const& config);

    [[nodiscard]] static queue_set_relations createQueueSetRelations(
        queue_set_t const& queueSet,
        std::span<QueueFamilyProperties const> queueProperties
    );

    /**
     * @return std::nullopt -> no physical device supports the queue set
     */
    [[nodiscard]] std::optional<queue_mappings> tryCreatePhysicalDevice(
        std::span<Surface const> surfaces,
        Config const& config,
        queue_set_t const& queueSet
    );

    void createQueues(std::span<size_t const> queuesToUniqueQueues);


    not_null<Instance const*> _instance;

    std::unique_ptr<PhysicalDevice> _physicalDevice;
    std::unique_ptr<Device> _device;
    std::optional<size_t> _queueSetIndex;
    std::vector<Queue> _queues;

public:
    Core(Core const& other) = delete;
    Core(Core&& other) noexcept;
    Core& operator=(Core const& other) = delete;
    Core& operator=(Core&& other) noexcept;

    static void debug_check(Core const& core);
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
     * @brief queue per entry of @ref Config::queueProperties
     * @details empty -> the queues of @ref device
     */
    std::vector<Queue> queues{};
    std::optional<size_t> queueSetIndex = std::nullopt;
};
}

namespace cvk {
inline void Core::debug_check(Core const& core) {
    CTH_CRITICAL(!core.created(), "core must be created") {}
}
}
