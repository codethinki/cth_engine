#pragma once
#include "jvk/base/queue/queue.hpp"
#include "jvk/surface/surface.hpp"
#include "jvk/utility/constants.hpp"

#include <volk.h>
#include <cth/pointers.hpp>

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
    struct Config;
    struct State;

    Core() = default;

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
    std::unique_ptr<Device> _device;
    std::unique_ptr<PhysicalDevice> _physicalDevice;
    std::unique_ptr<Instance> _instance;
    std::unique_ptr<DestructionQueue> _destructionQueue;

public:
    [[nodiscard]] bool created() const {
        return _device != nullptr && _physicalDevice != nullptr && _instance != nullptr;
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


//Config

namespace jvk {
struct Core::Config {
    std::string_view appName;
    std::string_view engineName;
    std::span<Queue> queues;
    std::span<std::string const> requiredExtensions; //TODO replace this with better extension handling

    /**
     * @brief if true, creates a DestructionQueue
     */
    bool destructionQueue;

    static Config Default(std::string_view app_name, std::string_view engine_name, std::span<Queue> queues,
        std::span<std::string const> required_extensions) {
        return Config{app_name, engine_name, queues, required_extensions, true};
    }
};
}


//debug check

namespace jvk {

inline void Core::debug_check(Core const& core) {
    CTH_CRITICAL(!core.created(), "core must be created") {}
}
}
