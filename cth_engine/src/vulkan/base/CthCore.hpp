#pragma once
#include "vulkan/base/queue/CthQueue.hpp"
#include "vulkan/utility/cth_constants.hpp"

#include <cth/pointers.hpp>
#include <volk.h>

#include <span>


namespace cth::vk {
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
     * @note calls @ref optDestroy()
     */
    explicit Core(State state);
    /**
     * @brief constructs and creates
     */
    explicit Core(Config const& config);

    /**
     * @note calls @ref optDestroy()
     */
    ~Core();

    /**
     * @brief wraps the @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State state);

    /**
     * @brief creates the components
     * @note calls @ref Instance::Instance(std::string_view, std::span<std::string const>, std::optional<PFN_vkDebugUtilsMessengerCallbackEXT>)
     * @note calls @ref PhysicalDevice::AutoPick(Instance*, std::span<Queue>, std::span<std::string const>, std::span<std::string const>)
     * @note calls @ref Device::Device(Instance*, PhysicalDevice*, std::span<Queue>)
     * @note may calls @ref DestructionQueue::DestructionQueue(Device*, PhysicalDevice*, Instance*)
     */
    void create(Config const& config);

    void destroy();
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief sets all components to nullptr
     * @note does not delete
     */
    void reset();

    State release();

private:
    std::unique_ptr<Device> _device;
    std::unique_ptr<PhysicalDevice> _physicalDevice;
    std::unique_ptr<Instance> _instance;
    std::unique_ptr<DestructionQueue> _destructionQueue;

public:
    [[nodiscard]] bool created() const { return _device != nullptr && _physicalDevice != nullptr && _instance != nullptr; }

    [[nodiscard]] Device const* device() const;
    [[nodiscard]] VkDevice vkDevice() const;
    [[nodiscard]] PhysicalDevice const* physicalDevice() const;
    [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const;
    [[nodiscard]] Instance const* instance() const;
    [[nodiscard]] VkInstance vkInstance() const;
    [[nodiscard]] DestructionQueue* destructionQueue() const;

    Core(Core const& other) = delete;
    Core(Core&& other) noexcept = default;
    Core& operator=(Core const& other) = delete;
    Core& operator=(Core&& other) noexcept = default;

    static void debug_check(cth::not_null<Core const*> core);
};


}


//State

namespace cth::vk {
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

namespace cth::vk {
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
        std::span<std::string const> required_extensions) { return Config{app_name, engine_name, queues, required_extensions, true}; }
};
}


//debug check

namespace cth::vk {

inline void Core::debug_check(cth::not_null<Core const*> core) {
    CTH_CRITICAL(!core->created(), "core must be created") {}
}
}
