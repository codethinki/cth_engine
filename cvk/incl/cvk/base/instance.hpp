#pragma once

#include "cvk/debug/debug_messenger.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/ptr.hpp>
#include <cth/utility.hpp>

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>



namespace cvk {
/**
 * @brief VkInstance wrapper, owns the debug messenger
 */
class Instance {
public:
    struct State;

    static constexpr std::array<char const*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };
    static constexpr std::array<char const*, 1> VALIDATION_LAYER_EXTENSIONS{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    static constexpr std::array<std::string_view, 0> REQUIRED_INSTANCE_EXTENSIONS{};

    /**
     * @brief base constructor
     * @details debug builds enable the validation layers
     * @throws cth::except::default_exception reason: required instance extension missing
     * @throws cth::except::default_exception reason: required validation layer missing
     */
    Instance(std::string_view appName, std::span<std::string const> requiredExtensions);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Instance(std::string_view, std::span<std::string const>)
     * - @ref create()
     */
    Instance(
        std::string_view appName,
        std::span<std::string const> requiredExtensions,
        std::optional<DebugMessenger::Config> const& messengerConfig
    );

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Instance(std::string_view, std::span<std::string const>)
     * - @ref wrap()
     */
    Instance(std::string_view appName, std::span<std::string const> requiredExtensions, State);

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
     * @brief creates the instance
     * @param messengerConfig std::nullopt -> no messenger, debug builds use @ref DebugMessenger::Config::Default()
     * @pre @ref Cvk::initialized()
     * @post @ref created()
     * @post volk uses the functions of this instance
     * @note calls @ref optDestroy()
     * @note no validation layers -> no messenger
     * @throws cvk::vk_result_exception result of vkCreateInstance()
     */
    void create(std::optional<DebugMessenger::Config> messengerConfig = std::nullopt);

    /**
     * @brief destroys the messenger and the instance and resets
     * @pre @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @throws cth::except::default_exception reason: required extension missing
     */
    void checkInstanceExtensionSupport();

    /**
     * @throws cth::except::default_exception reason: required validation layer missing
     */
    void checkValidationLayerSupport();

    /**
     * @brief adds the extensions of the validation layers to the required extensions
     * @note calls @ref checkValidationLayerSupport()
     */
    void enableValidationLayers();

    [[nodiscard]] static std::vector<std::string> getAvailableValidationLayers();
    [[nodiscard]] static std::vector<std::string> getAvailableInstanceExtensions();
    [[nodiscard]] VkApplicationInfo appInfo() const;

    static void destroy(VkInstance);

    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkInstance get() const { return _handle.get(); }
    [[nodiscard]] std::vector<std::string> availableExtensions() const { return _availableExt; }
    [[nodiscard]] std::vector<std::string> requiredExtensions() const { return _requiredExt; }
    [[nodiscard]] std::vector<std::string> availableValidationLayers() const { return _availableLayers; }

private:
    void reset();

    std::string _name;

    std::vector<std::string> _requiredExt{};
    std::vector<std::string> _availableExt;
    std::vector<std::string> _availableLayers{};

    std::unique_ptr<DebugMessenger> _debugMessenger = nullptr;
    move_ptr<VkInstance_T> _handle = VK_NULL_HANDLE;

    static void loadInstanceFunctions(vk_not_null<VkInstance>);

public:
    Instance(Instance const& other) = delete;
    Instance& operator=(Instance const& other) = delete;
    Instance(Instance&& other) noexcept = default;
    Instance& operator=(Instance&& other) noexcept { return cth::reconstruct_at(*this, std::move(other)); }

    static void debug_check(Instance const&);
    static void debug_check_handle(vk_not_null<VkInstance>);
};
}


namespace cvk {
struct Instance::State {
    vk_not_null<VkInstance> vkInstance;

    /**
     * @brief messenger of @ref vkInstance
     * @note may be nullptr
     */
    std::unique_ptr<DebugMessenger> debugMessenger;
};
}

namespace cvk {
inline void Instance::debug_check(Instance const& instance) { debug_check_handle(instance.get()); }
inline void Instance::debug_check_handle([[maybe_unused]] vk_not_null<VkInstance> vkInstance) {}
}
