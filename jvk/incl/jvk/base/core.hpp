#pragma once
#include "jvk/base/core_config.hpp"

#include "jvk/base/queue/queue.hpp"
#include "jvk/surface/surface.hpp"
#include "jvk/utility/constants.hpp"
#include "jvk/utility/types.hpp"
#include "jvk/utility/vk_exceptions.hpp"



#include <map>
#include <volk.h>
#include <cth/data/union_find.hpp>

#include <span>


namespace jvk {
struct DeviceTable;

class DestructionQueue;
class Queue;
class Device;
class PhysicalDevice;
class Instance;

class Core {
public:
    using Config = CoreConfig;
    using queue_set_t = Config::queue_set_t;
    struct State;

    Core();

    /**
     * @brief constructs and wraps
     * @details
     * calls: @ref optDestroy()
     */
    explicit Core(State state);

    /**
     * @brief constructs and creates
     * @details
     * calls: @ref create(vk_not_null<VkSurfaceKHR>, Config const&)
     */
    explicit Core(Config const& config);

    /**
     * @details
     * calls: @ref optDestroy()
     */
    ~Core();

    /**
     * @brief wraps the @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State state);

    /**
     * @brief creates the components
     * @details
     * calls:
     * - @ref Instance::Instance(std::string_view, std::span<std::string const>, std::optional<DebugMessenger::Config> const&)
     * - @ref PhysicalDevice::AutoPick(Instance const&, vk_not_null<VkSurfaceKHR>, std::span<Queue const>, std::span<std::string const>, utils::PhysicalDeviceFeatures const&)
     * - @ref Device::Device(Instance const&, PhysicalDevice const&, std::span<Queue>)
     * - if @ref Config::destructionQueue -> @ref DestructionQueue::DestructionQueue()
     */
    void create(Config const& config);

    /**
     * @brief destroys the objects state
     */
    void destroy();

    /** @brief destroys if @ref created() */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief resets
     * @details calls: @ref optDestroy()
     */
    void reset();

    /**
     * @brief releases the state
     * @return internal state
     * @details calls: @ref reset()
     *
     * @note does destroy state
     */
    State release();

private:
    /**
     * first: unique queues, second: queue -> unique queue mapping
     */
    struct queue_mappings {
        std::vector<queue_family_index_t> uniqueQueueFamilyIndices;
        std::vector<size_t> queuesToUniqueQueues;
    };

    /**
     * @post if successful, sets `_queueSetIndex`
     * @throws jvk::jvk_exception if no physical device meets the requirements
     */
    [[nodiscard]] queue_mappings createPhysicalDevice(
        std::span<QueueFamilyProperties const> queues,
        std::span<queue_set_t const> queue_sets
    );

    [[nodiscard]] std::optional<queue_mappings> tryCreatePhysicalDevice(
        std::span<Surface const> surfaces,
        std::span<QueueFamilyProperties const> queue_properties,
        queue_set_t const& queue_set
    );

    void createQueues(std::span<size_t const> queues_to_unique_queues);

    std::unique_ptr<DestructionQueue> _destructionQueue;
    std::unique_ptr<Instance> _instance;
    std::unique_ptr<PhysicalDevice> _physicalDevice;
    std::unique_ptr<Device> _device;
    std::optional<size_t> _queueSetIndex;
    std::vector<Queue> _queues;

public:
    [[nodiscard]] bool created() const {
        return _device != nullptr && _physicalDevice != nullptr && _instance != nullptr;
    }
    /**
     * @pre sets were provided in the config
     * @throws jvk::jvk_exception if no sets were given
     */
    [[nodiscard]] size_t queueSetIndex() const {
        debug_check(*this);
        CTH_CRITICAL(_queueSetIndex.has_value(), "queue set index has no value") {}
        return *_queueSetIndex;
    }

    [[nodiscard]] Device const& device() const;
    [[nodiscard]] DeviceTable deviceTable() const;
    [[nodiscard]] VolkDeviceTable const* functions() const;
    [[nodiscard]] VkDevice vkDevice() const;
    [[nodiscard]] PhysicalDevice const& physicalDevice() const;
    [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const;
    [[nodiscard]] Instance const& instance() const;
    [[nodiscard]] VkInstance vkInstance() const;
    [[nodiscard]] DestructionQueue* destructionQueue() const;
    [[nodiscard]] auto& queue(this auto& self, size_t idx) { return self._queues[idx]; }

    Core(Core const& other) = delete;
    Core(Core&& other) noexcept = default;
    Core& operator=(Core const& other) = delete;
    Core& operator=(Core&& other) noexcept = default;

    static void debug_check(Core const& core);
};


}


//State

namespace jvk {
struct Core::State {
    /**
     * @attention Instance::created() required
     */
    unique_not_null<Instance> instance;
    /**
     * @attention PhysicalDevice::created() required
     */
    unique_not_null<PhysicalDevice> physicalDevice;
    /**
     * @attention Device::created() required
     */
    unique_not_null<Device> device;
    /**
     *@brief optional but highly recommended
     * @note may be nullptr
     */
    std::unique_ptr<DestructionQueue> destructionQueue;
};
}



//debug check

namespace jvk {

inline void Core::debug_check(Core const& core) {
    CTH_CRITICAL(!core.created(), "core must be created") {}
}
}
