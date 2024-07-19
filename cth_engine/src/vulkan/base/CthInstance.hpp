#pragma once

#include "vulkan/utility/cth_constants.hpp"

#include "vulkan/debug/CthDebugMessenger.hpp"

#include<cth/cth_pointer.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <optional>
#include <span>
#include <vector>



namespace cth::vk {
class BasicDebugMessenger;

class DeletionQueue;


class BasicInstance {
public:
    BasicInstance(std::string_view app_name, std::span<const std::string> required_extensions);
    virtual ~BasicInstance() = default;

    /**
     * wraps an existing VkInstance
     */
    virtual void wrap(VkInstance vk_instance);

    /**
    * @throws cth::except::vk_result_exception result of vkCreateInstance()
    * @note debug_messenger will not be stored
    */
    virtual void create(const std::optional<BasicDebugMessenger::Config>& messenger_config = std::nullopt);

    void destroy();

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

protected:
    std::string _name;

    std::vector<std::string> _requiredExt{};
    std::vector<std::string> _availableExt;
    std::vector<std::string> _availableLayers{};

private:
    move_ptr<VkInstance_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkInstance get() const { return _handle.get(); }
    [[nodiscard]] std::vector<std::string> availableExtensions() const { return _availableExt; }
    [[nodiscard]] std::vector<std::string> requiredExtensions() const { return _availableExt; }
    [[nodiscard]] std::vector<std::string> availableValidationLayers() const { return _availableLayers; }

    static constexpr std::array<const char*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };
    static constexpr std::array<const char*, 1> VALIDATION_LAYER_EXTENSIONS{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    static constexpr std::array<std::string_view, 0> REQUIRED_INSTANCE_EXTENSIONS{};

    BasicInstance(const BasicInstance& other) = default;
    BasicInstance& operator=(const BasicInstance& other) = default;
    BasicInstance(BasicInstance&& other) noexcept = default;
    BasicInstance& operator=(BasicInstance&& other) noexcept = default;
#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const BasicInstance* instance);
    static void debug_check_handle(VkInstance vk_instance);
    static void debug_check_leak(const BasicInstance* instance);


#define DEBUG_CHECK_INSTANCE(instance_ptr) BasicInstance::debug_check(instance_ptr)
#define DEBUG_CHECK_INSTANCE_HANDLE(instance_ptr) BasicInstance::debug_check_handle(instance_ptr)
#define DEBUG_CHECK_INSTANCE_LEAK(instance_ptr) BasicInstance::debug_check_leak(instance_ptr)
#else
#define DEBUG_CHECK_INSTANCE(instance_ptr) ((void)0)
#define DEBUG_CHECK_INSTANCE_LEAK(instance_ptr) ((void)0)
#endif
};
}

namespace cth::vk {

class Instance : public BasicInstance {
public:
    /**
    * @throws cth::except::vk_result_exception result vkCreateInstance()
    * @throws cth::except::default_exception reason: missing required instance extensions
    * @throws cth::except::default_exception reason: missing required validation layers
    */
    explicit Instance(std::string_view app_name, const std::span<const std::string> required_extensions);
    ~Instance() override;

    void wrap(VkInstance vk_instance) override;

    void create(const std::optional<BasicDebugMessenger::Config>& messenger_config) override;

private:
#ifdef CONSTANT_DEBUG_MODE
    std::unique_ptr<DebugMessenger> _debugMessenger = nullptr;
#endif

public:
    Instance(const Instance& other) = delete;
    Instance(Instance&& other) noexcept = default;
    Instance& operator=(const Instance& other) = delete;
    Instance& operator=(Instance&& other) noexcept = default;
};
}
