#pragma once
#include "instance_config.hpp"

#include "cvk/debug/debug_messenger.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/handle/unique_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/ptr.hpp>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>


namespace cvk {
/**
 * `VkInstance` wrapper
 * @details owns the debug messenger
 */
class Instance {
public:
    using Config = InstanceConfig;
    struct State;

    static constexpr std::array<char const*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };
    /**
     * extensions that a messenger needs
     */
    static constexpr std::array<char const*, 1> MESSENGER_EXTENSIONS{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    static constexpr VkBool32 VALIDATE_SYNC = VK_TRUE;

    /**
     * settings of the validation layers
     * @details `validate_sync`: synchronization validation
     */
    static constexpr std::array<VkLayerSettingEXT, 1> VALIDATION_LAYER_SETTINGS{
        VkLayerSettingEXT{
            .pLayerName = VALIDATION_LAYERS[0],
            .pSettingName = "validate_sync",
            .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
            .valueCount = 1,
            .pValues = &VALIDATE_SYNC,
        },
    };

    /**
     * base constructor
     */
    explicit Instance(Config);

    /**
     * constructs and wraps
     * @details calls `Instance(Config)`, `wrap()`
     */
    Instance(Config, State);

    /**
     * constructs and creates
     * @details calls `Instance(Config)`, `create()`
     */
    explicit Instance(Config, create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~Instance() { optDestroy(); }

    /**
     * wraps the state
     * @details the messenger of the state gets the config of the messenger, see `Config::debugMessenger`
     * @pre the state has a messenger <=> the instance has a messenger config, debug builds always have one
     * @post `created()`
     * @post volk uses the functions of this instance
     * @note calls `optDestroy()`
     */
    void wrap(State);

    /**
     * creates the instance
     * @details enables the extensions of the config
     * @details debug builds: enables the validation layers
     * @details a messenger config, see `Config::debugMessenger` -> enables `MESSENGER_EXTENSIONS`
     * @details a messenger config -> creates the messenger
     * @pre `Cvk::initialized()`
     * @post `created()`
     * @post volk uses the functions of this instance
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: instance extension missing
     * @throws cvk::cvk_exception reason: validation layer missing
     * @throws cvk::vk_result_exception result of `vkEnumerateInstanceExtensionProperties()`
     * @throws cvk::vk_result_exception result of `vkEnumerateInstanceLayerProperties()`
     * @throws cvk::vk_result_exception result of `vkCreateInstance()`
     * @throws cvk::vk_result_exception result of `vkCreateDebugUtilsMessengerEXT()`, destroys the instance
     */
    void create();

    /**
     * destroys the messenger and the instance and forgets
     * @pre `created()`
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership of the instance and the messenger, returns the state and forgets
     * @pre `created()`
     */
    State release();

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkInstance get() const { return _handle.get(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return the enabled extensions, a messenger adds `MESSENGER_EXTENSIONS`
     */
    [[nodiscard]] constexpr std::span<std::string const> extensions() const { return _extensions; }

    /**
     * @return `nullptr` -> no messenger
     */
    [[nodiscard]] constexpr DebugMessenger const* debugMessenger() const {
        return _debugMessengerOpt.has_value() ? &*_debugMessengerOpt : nullptr;
    }

private:
    void forget();

    /**
     * the config the messenger is created with
     * @return `std::nullopt` -> no messenger, debug builds: the config of `Config::debugMessenger` or the default
     */
    [[nodiscard]] constexpr std::optional<DebugMessenger::Config> messengerConfig() const;

    static void loadInstanceFunctions(vk_not_null<VkInstance>);

    Config _config;
    std::vector<std::string> _extensions{};

    // declared before the handle: move assignment destroys the old messenger before the old instance
    std::optional<DebugMessenger> _debugMessengerOpt = std::nullopt;
    unique_handle<VkInstance, handle_deleter> _handle;

public:
    Instance(Instance const& other) = delete;
    Instance& operator=(Instance const& other) = delete;
    Instance(Instance&& other) noexcept = default;
    Instance& operator=(Instance&& other) noexcept = default;

    static constexpr void debug_check(Instance const&);
    static constexpr void debug_check_handle(vk_not_null<VkInstance>);
};

/**
 * queries the instance extensions of the loader and the implicit layers
 * @pre `Cvk::initialized()`
 * @throws cvk::vk_result_exception result of `vkEnumerateInstanceExtensionProperties()`
 */
[[nodiscard]] std::vector<std::string> available_instance_extensions();

/**
 * queries the instance layers
 * @pre `Cvk::initialized()`
 * @throws cvk::vk_result_exception result of `vkEnumerateInstanceLayerProperties()`
 */
[[nodiscard]] std::vector<std::string> available_layers();
}


namespace cvk {
struct Instance::State {
    vk_not_null<VkInstance> vkInstance;

    /**
     * messenger of `vkInstance`, `std::nullopt` -> no messenger
     */
    std::optional<DebugMessenger::State> debugMessenger = std::nullopt;

    std::vector<std::string> extensions{}; ///< the extensions that the instance enables
};
}

namespace cvk {
constexpr std::optional<DebugMessenger::Config> Instance::messengerConfig() const {
    if constexpr(constants::ENABLE_VALIDATION_LAYERS) return _config.debugMessenger.value_or(DebugMessenger::Config{});
    else return _config.debugMessenger;
}
}

namespace cvk {
constexpr void Instance::debug_check(Instance const& instance) {
    CTH_CRITICAL(!instance.created(), "instance must be created") {}
}
constexpr void Instance::debug_check_handle([[maybe_unused]] vk_not_null<VkInstance> vkInstance) {}
}
