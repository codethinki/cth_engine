#pragma once
#include "CthQueue.hpp"
#include "vulkan/utility/cth_constants.hpp"

#include<cth/cth_pointer.hpp>
#include <vulkan/vulkan.h>

#include <span>

namespace cth::vk {
class BasicInstance;
class PhysicalDevice;
class Device;
class Queue;

class BasicCore {
public:
    struct Config;

    BasicCore() = default;

    virtual void wrap(move_ptr<BasicInstance> instance, move_ptr<PhysicalDevice> physical_device, move_ptr<Device> device);

    virtual void create(Config const& config);
    void destroy();

    /**
     * @brief sets all components to nullptr
     * @note does not delete
     */
    virtual void reset();

private:
    move_ptr<Device> _device = nullptr;
    move_ptr<PhysicalDevice> _physicalDevice = nullptr;
    move_ptr<BasicInstance> _instance = nullptr;

public:
    [[nodiscard]] Device const* device() const;
    [[nodiscard]] VkDevice vkDevice() const;
    [[nodiscard]] PhysicalDevice const* physicalDevice() const;
    [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const;
    [[nodiscard]] BasicInstance const* instance() const;
    [[nodiscard]] VkInstance vkInstance() const;

    bool operator==(BasicCore const& other) const {
        return _device == other._device && _physicalDevice == other._physicalDevice && _instance == other._instance;
    }

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(BasicCore const* core);
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
    virtual ~Core();

    /**
     * @brief wraps and takes ownership
     */
    void wrap(move_ptr<BasicInstance> instance, move_ptr<PhysicalDevice> physical_device, move_ptr<Device> device) override;
    /**
     * @brief creates the components
     * @note not necessary when using wrap
     */
    void create(Config const& config) override;

    /**
     * @brief deletes all components
     */
    void reset() override;

    void release();
};
}

//Config

namespace cth::vk {
struct BasicCore::Config {
    std::string_view appName;
    std::string_view engineName;
    std::span<Queue> queues;
    std::span<std::string const> requiredExtensions; //TEMP

    static Config Default(std::string_view const app_name, std::string_view const engine_name, std::span<Queue> const queues, std::span<std::string const> const required_extensions) {
        return Config{app_name, engine_name, queues, required_extensions};
    }
};
}
