#pragma once
#include <array>
#include <memory>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>


namespace cth {
using namespace std;

class DebugMessenger;

class Instance {
public:
    static constexpr array<const char*, 1> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };


    explicit Instance(const string& name, const vector<const char*>& required_extensions);
    ~Instance();

    [[nodiscard]] VkInstance get() const { return vkInstance; }

    [[nodiscard]] VkApplicationInfo appInfo() const;


    [[nodiscard]] vector<const char*> availableExtensions() const { return availableExt; }
    [[nodiscard]] vector<const char*> requiredExtensions() const { return availableExt; }
    [[nodiscard]] vector<const char*> availableValidationLayers() const { return availableLayers; }

private:
    [[nodiscard]] VkInstanceCreateInfo createInfo() const;
    /**
     * \throws cth::except::default_exception reason: required extension not supported
     */
    void checkInstanceExtensionsSupport();
    /**
     * \throws cth::except::default_exception reason: required layers not supported
     */
    void checkValidationLayersSupport();
    [[nodiscard]] static vector<const char*> getAvailableValidationLayers();

    string name;

    vector<const char*> requiredExt;
    vector<const char*> availableExt;
    vector<const char*> availableLayers{};

    VkInstance vkInstance = VK_NULL_HANDLE;
    unique_ptr<DebugMessenger> debugMessenger = nullptr;



    static vector<const char*> getAvailableInstanceExtensions();

public:
    static constexpr bool ENABLE_VALIDATION_LAYERS = []() {
#ifdef NDEBUG
        return false;
#else
        return true;
#endif
    }();


    // Not copyable or movable
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;
};
}
