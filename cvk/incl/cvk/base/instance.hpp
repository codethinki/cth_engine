#pragma once
#include "instance_config.hpp"

#include "cvk/debug/debug_messenger.hpp"
#include "cvk/utility/constants.hpp"
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
 * @brief VkInstance wrapper, owns the debug messenger
 */
class Instance {
public:
    using vk_handle_t = VkInstance;
    using Config = InstanceConfig;
    struct State;

    static constexpr std::array<char const*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };
    static constexpr std::array<char const*, 1> VALIDATION_LAYER_EXTENSIONS{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    static constexpr VkBool32 VALIDATE_SYNC = VK_TRUE;

    /**
     * @brief settings of the validation layers
     * @details validate_sync: reports missing and too narrow barriers, semaphores and other dependencies
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
     * @brief base constructor
     */
    explicit Instance(Config);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Instance(Config)
     * - @ref wrap()
     */
    Instance(Config, State);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Instance(Config)
     * - @ref create()
     */
    explicit Instance(Config, create_t, std::optional<DebugMessenger::Config> messengerConfig = std::nullopt);

    /**
     * @note calls @ref optDestroy()
     */
    ~Instance() { optDestroy(); }

    /**
     * @brief wraps the state
     * @post @ref created()
     * @post volk uses the functions of this instance
     * @note calls @ref optDestroy()
     */
    void wrap(State);

    /**
     * @brief creates the instance with the extensions of the config, debug builds with the validation layers
     * @param messengerConfig std::nullopt -> no messenger, debug builds use @ref DebugMessenger::Config::Default()
     * @pre @ref Cvk::initialized()
     * @post @ref created()
     * @post volk uses the functions of this instance
     * @note calls @ref optDestroy()
     * @note no validation layers -> no messenger
     * @throws cvk::cvk_exception reason: instance extension missing
     * @throws cvk::cvk_exception reason: validation layer missing
     * @throws cvk::vk_result_exception result of vkCreateInstance()
     */
    void create(std::optional<DebugMessenger::Config> messengerConfig = std::nullopt);

    /**
     * @brief destroys the messenger and the instance and forgets
     * @pre @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership of the instance and the messenger, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @pre volk loaded the instance functions, @ref create() and @ref wrap() load them
     */
    static void destroy(VkInstance);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return the enabled extensions, debug builds include @ref VALIDATION_LAYER_EXTENSIONS
     */
    [[nodiscard]] constexpr std::span<std::string const> extensions() const { return _extensions; }

    /**
     * @return nullptr -> no messenger
     */
    [[nodiscard]] constexpr DebugMessenger const* debugMessenger() const {
        return _debugMessenger.has_value() ? &*_debugMessenger : nullptr;
    }

private:
    void forget();

    static void loadInstanceFunctions(vk_not_null<VkInstance>);

    Config _config;
    std::vector<std::string> _extensions{};

    // declared before the handle: move assignment destroys the old messenger before the old instance
    std::optional<DebugMessenger> _debugMessenger = std::nullopt;
    unique_handle<vk_handle_t, handle_deleter<Instance>> _handle;

public:
    Instance(Instance const& other) = delete;
    Instance& operator=(Instance const& other) = delete;
    Instance(Instance&& other) noexcept = default;
    Instance& operator=(Instance&& other) noexcept = default;

    static constexpr void debug_check(Instance const&);
    static constexpr void debug_check_handle(vk_not_null<VkInstance>);
};

/**
 * @brief queries the instance extensions of the loader and the implicit layers
 * @pre @ref Cvk::initialized()
 */
[[nodiscard]] std::vector<std::string> available_instance_extensions();

/**
 * @brief queries the instance layers
 * @pre @ref Cvk::initialized()
 */
[[nodiscard]] std::vector<std::string> available_layers();
}


namespace cvk {
struct Instance::State {
    vk_not_null<VkInstance> vkInstance;

    /**
     * @brief messenger of @ref vkInstance, std::nullopt -> no messenger
     */
    std::optional<DebugMessenger> debugMessenger = std::nullopt;

    /**
     * @brief the extensions that the instance enables
     */
    std::vector<std::string> extensions{};
};
}

namespace cvk {
constexpr void Instance::debug_check(Instance const& instance) {
    CTH_CRITICAL(!instance.created(), "instance must be created") {}
}
constexpr void Instance::debug_check_handle([[maybe_unused]] vk_not_null<VkInstance> vkInstance) {}
}
