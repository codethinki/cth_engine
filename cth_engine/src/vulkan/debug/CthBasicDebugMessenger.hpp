#pragma once
#include "vulkan/utility/CthConstants.hpp"

#include<cth/cth_pointer.hpp>

#include <vulkan/vulkan.h>

#include <functional>

namespace cth::dev {
VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);
}

namespace cth {
class BasicInstance;
class DeletionQueue;

class BasicDebugMessenger {
public:
    struct Config;
    using callback_t = VkBool32(const VkDebugUtilsMessageSeverityFlagBitsEXT, const VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT*,
        void*);


    /**
    * @note implicitly calls create();
    */
    explicit BasicDebugMessenger(Config config) : _config{std::move(config)} {}
    virtual ~BasicDebugMessenger() = default;

    /**
     * @throws cth::except::default_exception reason: messenger already active
     * @throws cth::except::default_exception reason: vkGetInstanceProcAddr() returned nullptr
     * @throws cth::except::vk_result_exception result of vkCreateDebugUtilsMessengerEXT()
     */
    virtual void create(const BasicInstance* instance);

    /**
     * @throws cth::except::default_exception reason: messenger not active
     * @throws cth::except::default_exception reason: vkGetInstanceProcAddr() returned nullptr
     */
    virtual void destroy();


    static void destroy(VkInstance instance, VkDebugUtilsMessengerEXT vk_messenger);

    struct Config {
        std::function<callback_t> callback = dev::defaultDebugCallback;
        VkDebugUtilsMessageSeverityFlagsEXT messageSeverities = Constant::DEBUG_MESSAGE_SEVERITY;
        VkDebugUtilsMessageTypeFlagsEXT messageTypes = Constant::DEBUG_MESSAGE_TYPE;


        static Config Default(const std::function<callback_t>& callback = nullptr) {
            return Config{
                .callback = callback == nullptr ? dev::defaultDebugCallback : callback,
            };
        }


        [[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT createInfo() const;
    };

protected:
    const BasicInstance* _instance = nullptr;
    Config _config;

private:
    ptr::mover<VkDebugUtilsMessengerEXT_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkDebugUtilsMessengerEXT get() const { return _handle.get(); }
    [[nodiscard]] Config config() const { return _config; }

    BasicDebugMessenger(const BasicDebugMessenger&) = default;
    BasicDebugMessenger& operator=(const BasicDebugMessenger&) = default;
    BasicDebugMessenger(BasicDebugMessenger&&) = default;
    BasicDebugMessenger& operator=(BasicDebugMessenger&&) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const BasicDebugMessenger* debug_messenger);
    static void debug_check_leak(const BasicDebugMessenger* debug_messenger);

#define DEBUG_CHECK_MESSENGER(messenger_ptr) BasicDebugMessenger::debug_check(messenger_ptr)
#define DEBUG_CHECK_MESSENGER_LEAK(messenger_ptr) BasicDebugMessenger::debug_check_leak(messenger_ptr)
#else
#define DEBUG_CHECK_MESSENGER(messenger_ptr) ((void)0)
#define DEBUG_CHECK_MESSENGER_LEAK(messenger_ptr) ((void)0)
#endif


};
} // namespace cth
