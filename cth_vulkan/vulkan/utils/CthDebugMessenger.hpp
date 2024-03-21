#pragma once
#include <functional>
#include <vulkan/vulkan.h>


namespace cth {
using namespace std;
class Instance;

class DebugMessenger {
public:
    using callback_t = VkBool32(const VkDebugUtilsMessageSeverityFlagBitsEXT, const VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT*,
        void*);

    /**
     * \param custom_callback == nullptr -> default callback function
     */
    explicit DebugMessenger(const function<callback_t>& custom_callback = nullptr);
    /**
 * \param custom_callback == nullptr -> default callback function
 * \throws cth::except::default_exception
 * \throws cth::except::vk_result_exception
 */
    explicit DebugMessenger(Instance* instance, const function<callback_t>& custom_callback = nullptr);
    ~DebugMessenger();


    /**
     * \throws cth::except::default_exception reason: messenger already active
     * \throws cth::except::default_exception reason: vkGetInstanceProcAddr() returned nullptr
     * \throws cth::except::vk_result_exception result of vkCreateDebugUtilsMessengerEXT()
     */
    void init(Instance* instance);

    /**
     * \throws cth::except::default_exception reason: messenger not active
     * \throws cth::except::default_exception reason: vkGetInstanceProcAddr() returned nullptr
     */
    void destroy();

    [[nodiscard]] VkDebugUtilsMessengerEXT get() const { return vkMessenger; }

    [[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT createInfo() const;

private:
    void setCallback(const function<callback_t>& custom_callback);

    function<callback_t> callback;
    Instance* instance = nullptr;
    VkDebugUtilsMessengerEXT vkMessenger = VK_NULL_HANDLE;

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

