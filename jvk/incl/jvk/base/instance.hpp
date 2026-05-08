#pragma once

#include "jvk/debug/debug_messenger.hpp"
#include "jvk/utility/constants.hpp"
#include "jvk/utility/types.hpp"

#include <volk.h>
#include <cth/ptr.hpp>

#include <array>
#include <optional>
#include <span>
#include <vector>



namespace jvk {
class DebugMessenger;

class DestructionQueue;


class Instance {
public:
    struct State;

    /**
     * @brief base constructor
     * @throws cth::except::default_exception reason: missing required instance extensions
     * @throws cth::except::default_exception reason: missing required validation layers
     */
    Instance(std::string_view app_name, std::span<std::string const> required_extensions);

    /**
     * @brief constructs and creates
     * @note calls @ref Instance(std::string_view, std::span<std::string const>)
     * @note calls @ref create()
     */
    Instance(
        std::string_view app_name,
        std::span<std::string const> required_extensions,
        std::optional<DebugMessenger::Config> const& messenger_config
    );

    /**
     * @brief constructs and wraps
     *  @note calls @ref Instance(std::string_view, std::span<std::string const>)
     *  @note calls @ref wrap()
     */
    Instance(std::string_view app_name, std::span<std::string const> required_extensions, State state);

    ~Instance() { optDestroy(); }

    /**
     * @brief wraps state
     * @note calls @ref optDestroy()
     */
    void wrap(State state);

    /**
     * @brief creates the instance
     * @param messenger_config if not std::nullopt creates messenger with config
     * @note calls @ref optDestroy()
     * @throws jvk::result_exception result of @ref vkCreateInstance()
     */
    void create(std::optional<DebugMessenger::Config> messenger_config = std::nullopt);

    /**
     * @brief destroys the instance
     * @note requires @ref created()
     */
    void destroy();
    void optDestroy() { if(created()) destroy(); }

    /**
     * @throws cth::except::default_exception reason: required extension not supported
     */
    void checkInstanceExtensionSupport();

    /**
     * @throws cth::except::default_exception reason: required layers not supported
     */
    void checkValidationLayerSupport();

    void enableValidationLayers();

    [[nodiscard]] static std::vector<std::string> getAvailableValidationLayers();
    [[nodiscard]] static std::vector<std::string> getAvailableInstanceExtensions();
    [[nodiscard]] VkApplicationInfo appInfo() const;

    static void destroy(VkInstance vk_instance);

private:
    void reset();

    std::string _name;

    std::vector<std::string> _requiredExt{};
    std::vector<std::string> _availableExt;
    std::vector<std::string> _availableLayers{};

    std::unique_ptr<DebugMessenger> _debugMessenger = nullptr;
    move_ptr<VkInstance_T> _handle = VK_NULL_HANDLE;

    static void loadInstanceFunctions(jvk::vk_not_null<VkInstance> vk_instance);

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkInstance get() const { return _handle.get(); }
    [[nodiscard]] std::vector<std::string> availableExtensions() const { return _availableExt; }
    [[nodiscard]] std::vector<std::string> requiredExtensions() const { return _availableExt; }
    [[nodiscard]] std::vector<std::string> availableValidationLayers() const { return _availableLayers; }

    static constexpr std::array<char const*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };
    static constexpr std::array<char const*, 1> VALIDATION_LAYER_EXTENSIONS{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    static constexpr std::array<std::string_view, 0> REQUIRED_INSTANCE_EXTENSIONS{};

    Instance(Instance const& other) = delete;
    Instance& operator=(Instance const& other) = delete;
    Instance(Instance&& other) noexcept = default;
    Instance& operator=(Instance&& other) noexcept = default;


    static void debug_check(Instance const& instance);
    static void debug_check_handle(jvk::vk_not_null<VkInstance> vk_instance);
};


}


//State

namespace jvk {
struct Instance::State {
    jvk::vk_not_null<VkInstance> vkInstance;

    /**
     * @brief to @ref vkInstance attached debug messenger
     * @note may be nullptr
     */
    std::unique_ptr<DebugMessenger> debugMessenger;
};

}

//debug checks

namespace jvk {

inline void Instance::debug_check(Instance const& instance) { debug_check_handle(instance.get()); }
inline void Instance::debug_check_handle([[maybe_unused]] jvk::vk_not_null<VkInstance> vk_instance) {}
}
