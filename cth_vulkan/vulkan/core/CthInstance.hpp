#pragma once
#include "../utils/CthDebugMessenger.hpp"

#include <array>
#include <memory>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>


namespace cth {
using namespace std;


class Instance {
public:
    static constexpr array<const char*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };
    static constexpr array<const char*, 1> VALIDATION_LAYER_EXTENSIONS{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    [[nodiscard]] VkInstance get() const { return vkInstance; }

    [[nodiscard]] VkApplicationInfo appInfo() const;


    [[nodiscard]] vector<string> availableExtensions() const { return availableExt; }
    [[nodiscard]] vector<string> requiredExtensions() const { return availableExt; }
    [[nodiscard]] vector<string> availableValidationLayers() const { return availableLayers; }

private:
    /**
     * \throws cth::except::vk_result_exception result of vkCreateInstance()
     */
    void create();
    /**
     * \throws cth::except::default_exception reason: required extension not supported
     */
    void checkInstanceExtensionSupport();
    /**
     * \throws cth::except::default_exception reason: required layers not supported
     */
    void checkValidationLayerSupport();
    [[nodiscard]] static vector<string> getAvailableValidationLayers();

    string name;

    vector<string> requiredExt;
    vector<string> availableExt;
    vector<string> availableLayers{};

    VkInstance vkInstance = VK_NULL_HANDLE;
    unique_ptr<DebugMessenger> debugMessenger = nullptr;



    [[nodiscard]] static vector<string> getAvailableInstanceExtensions();
public:
    static constexpr bool ENABLE_VALIDATION_LAYERS = []() {
#ifdef NDEBUG
        return false;
#else
        return true;
#endif
    }();
    /**
    * \throws cth::except::vk_result_exception result vkCreateInstance()
    * \throws cth::except::default_exception reason: missing required instance extensions
    * \throws cth::except::default_exception reason: missing required validation layers
    */
    explicit Instance(string app_name, const vector<string>& required_extensions);
    ~Instance();

    // Not copyable or movable
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;
};
}
