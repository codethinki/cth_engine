#pragma once
#include "CthQueue.hpp"
#include "vulkan/utility/cth_constants.hpp"

#include<cth/cth_pointer.hpp>
#include <vulkan/vulkan.h>

#include <span>

namespace cth::vk {
class Instance;
class PhysicalDevice;
class Device;
class Queue;

class BasicCore {
public:
    struct Config;

    BasicCore() = default;
    virtual ~BasicCore() = default;

    /**
     * @brief wraps the components
     * @param destruction_queue optional
     */
    virtual void wrap(Instance* instance, PhysicalDevice* physical_device, Device* device, DestructionQueue* destruction_queue = nullptr);

    virtual void create(Config const& config);
    void destroy();

    /**
     * @brief sets all components to nullptr
     * @note does not delete
     */
    virtual void reset();

    struct State {
        Instance* instance;
        PhysicalDevice* physicalDevice;
        Device* device;
        DestructionQueue* destructionQueue;
    };
    State release();

private:
    move_ptr<Device> _device = nullptr;
    move_ptr<PhysicalDevice> _physicalDevice = nullptr;
    move_ptr<Instance> _instance = nullptr;
    move_ptr<DestructionQueue> _destructionQueue = nullptr;

public:
    [[nodiscard]] Device const* device() const;
    [[nodiscard]] VkDevice vkDevice() const;
    [[nodiscard]] PhysicalDevice const* physicalDevice() const;
    [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const;
    [[nodiscard]] Instance const* instance() const;
    [[nodiscard]] VkInstance vkInstance() const;
    [[nodiscard]] DestructionQueue* destructionQueue() const;

    bool operator==(BasicCore const& other) const {
        return _device == other._device && _physicalDevice == other._physicalDevice && _instance == other._instance;
    }

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(not_null<BasicCore const*> core);
    static void debug_check_leak(BasicCore const* core);
#define DEBUG_CHECK_CORE(core_ptr) BasicCore::debug_check(core_ptr)
#define DEBUG_CHECK_CORE_LEAK(core_ptr) BasicCore::debug_check_leak(core_ptr)
#else

#define DEBUG_CHECK_CORE(core_ptr) ((void)0)
#endif
};


}


namespace cth::vk {
class Instance;

class Core : public BasicCore {
public:
    //TEMP left off here
    explicit Core(Config const& config);
    ~Core() override;

    /**
     * @brief wraps and takes ownership
     */
    void wrap(Instance* instance, PhysicalDevice* physical_device, Device* device, DestructionQueue* destruction_queue) override;
    /**
     * @brief creates the components
     * @note not necessary when using wrap
     */
    void create(Config const& config) override;

    /**
     * @brief deletes all components
     */
    void reset() override;
};
}

//Config

namespace cth::vk {
struct BasicCore::Config {
    std::string_view appName;
    std::string_view engineName;
    std::span<Queue> queues;
    std::span<std::string const> requiredExtensions; //TEMP
    bool destructionQueue;

    static Config Default(std::string_view app_name, std::string_view engine_name, std::span<Queue> queues,
        std::span<std::string const> required_extensions) { return Config{app_name, engine_name, queues, required_extensions, true}; }
};
}
