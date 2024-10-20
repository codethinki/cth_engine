#pragma once

#include "vulkan/debug/CthDebugMessenger.hpp"
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

#include <cth/pointers.hpp>
#include <volk.h>

#include <array>
#include <optional>
#include <span>
#include <vector>



namespace cth::vk {
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
    Instance(std::string_view app_name, std::span<std::string const> required_extensions,
        std::optional<DebugMessenger::Config> const& messenger_config);

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
    * @throws cth::vk::result_exception result of @ref vkCreateInstance()
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

    static void addInstance(cth::vk::not_null<VkInstance> vk_instance);

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
#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(cth::not_null<Instance const*> instance);
    static void debug_check_handle(vk::not_null<VkInstance> vk_instance);


#define DEBUG_CHECK_INSTANCE(instance_ptr) Instance::debug_check(instance_ptr)
#define DEBUG_CHECK_INSTANCE_HANDLE(instance_ptr) Instance::debug_check_handle(instance_ptr)
#else
#define DEBUG_CHECK_INSTANCE(instance_ptr) ((void)0)
#define DEBUG_CHECK_INSTANCE_LEAK(instance_ptr) ((void)0)
#endif
};


}


//State

namespace cth::vk {
struct Instance::State {
    vk::not_null<VkInstance> vkInstance;

    /**
     * @brief to @ref vkInstance attached debug messenger
     * @note may be nullptr
     */
    std::unique_ptr<DebugMessenger> debugMessenger;
};

}
