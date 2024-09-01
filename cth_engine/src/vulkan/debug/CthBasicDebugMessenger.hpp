#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include<cth/cth_pointer.hpp>

#include <vulkan/vulkan.h>

#include <functional>

namespace cth::dev {
VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type, VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    void* user_data);
}

namespace cth::vk {
class BasicInstance;
class DestructionQueue;

class BasicDebugMessenger {
public:
    struct Config;
    using callback_t = VkBool32(VkDebugUtilsMessageSeverityFlagBitsEXT const, VkDebugUtilsMessageTypeFlagsEXT const,
        VkDebugUtilsMessengerCallbackDataEXT const*,
        void*);


    /**
    * @note calls create();
    */
    explicit BasicDebugMessenger(Config config) : _config{std::move(config)} {}
    virtual ~BasicDebugMessenger() = default;

    /**
     * @throws cth::except::default_exception reason: messenger already active
     * @throws cth::except::default_exception reason: vkGetInstanceProcAddr() returned nullptr
     * @throws cth::except::vk_result_exception result of vkCreateDebugUtilsMessengerEXT()
     */
    virtual void create(BasicInstance const* instance);

    /**
     * @throws cth::except::default_exception reason: messenger not active
     * @throws cth::except::default_exception reason: vkGetInstanceProcAddr() returned nullptr
     */
    virtual void destroy();


    static void destroy(VkInstance instance, VkDebugUtilsMessengerEXT vk_messenger);

    struct Config {
        std::function<callback_t> callback = dev::defaultDebugCallback;
        VkDebugUtilsMessageSeverityFlagsEXT messageSeverities = constants::DEBUG_MESSAGE_SEVERITY;
        VkDebugUtilsMessageTypeFlagsEXT messageTypes = constants::DEBUG_MESSAGE_TYPE;


        static Config Default(std::function<callback_t> const& callback = nullptr) {
            return Config{
                .callback = callback == nullptr ? dev::defaultDebugCallback : callback,
            };
        }


        [[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT createInfo() const;
    };

protected:
    BasicInstance const* _instance = nullptr;
    Config _config;

private:
    move_ptr<VkDebugUtilsMessengerEXT_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkDebugUtilsMessengerEXT get() const { return _handle.get(); }
    [[nodiscard]] Config config() const { return _config; }

    BasicDebugMessenger(BasicDebugMessenger const&) = default;
    BasicDebugMessenger& operator=(BasicDebugMessenger const&) = default;
    BasicDebugMessenger(BasicDebugMessenger&&) = default;
    BasicDebugMessenger& operator=(BasicDebugMessenger&&) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(BasicDebugMessenger const* debug_messenger);
    static void debug_check_leak(BasicDebugMessenger const* debug_messenger);

#define DEBUG_CHECK_MESSENGER(messenger_ptr) BasicDebugMessenger::debug_check(messenger_ptr)
#define DEBUG_CHECK_MESSENGER_LEAK(messenger_ptr) BasicDebugMessenger::debug_check_leak(messenger_ptr)
#else
#define DEBUG_CHECK_MESSENGER(messenger_ptr) ((void)0)
#define DEBUG_CHECK_MESSENGER_LEAK(messenger_ptr) ((void)0)
#endif


};
} // namespace cth
