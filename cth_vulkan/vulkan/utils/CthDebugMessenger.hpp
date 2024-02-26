#pragma once
#include <functional>
#include <cth/cth_log.hpp>
#include <vulkan/vulkan.h>


namespace cth {
using namespace std;
class Instance;

class DebugMessenger {
public:
    using callback_t = VkBool32(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT*,
        void*);

    explicit DebugMessenger(const function<callback_t>& callback = nullptr);
    explicit DebugMessenger(VkInstance instance, const function<callback_t>& callback = nullptr);
    ~DebugMessenger();


    /**
     * \throws cth::except::default_exception reason: messenger already active
     */
    void init(VkInstance instance);
    template<bool Throw>
    void destroy(VkInstance instance) const;

    [[nodiscard]] VkDebugUtilsMessengerEXT get() const { return vkMessenger; }

    [[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT createInfo() const;

private:
    void setCallback(const function<callback_t>& callback);

    function<callback_t> callback;
    VkInstance vkInstance = nullptr;
    VkDebugUtilsMessengerEXT vkMessenger = VK_NULL_HANDLE;

    bool active = false;

public:
    // Not copyable or movable
    DebugMessenger(const DebugMessenger&) = delete;
    DebugMessenger& operator=(const DebugMessenger&) = delete;
    DebugMessenger(DebugMessenger&&) = delete;
    DebugMessenger& operator=(DebugMessenger&&) = delete;
};
} // namespace cth
namespace cth::dev {
VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);
}

