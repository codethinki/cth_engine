#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include<cth/cth_pointer.hpp>

#include <gsl/pointers>
#include <vulkan/vulkan.h>

#include <functional>

namespace cth::dev {
VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type, VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    void* user_data);
}

namespace cth::vk {
class Instance;
class DestructionQueue;

class DebugMessenger {
public:
    struct Config;
    struct State;
    using callback_t = VkBool32(VkDebugUtilsMessageSeverityFlagBitsEXT const, VkDebugUtilsMessageTypeFlagsEXT const,
        VkDebugUtilsMessengerCallbackDataEXT const*,
        void*);



    /**
     * @brief base constructor 
     */
    explicit DebugMessenger(Config config);

    /**
     * @brief constructs and creates
     * @note calls @ref DebugMessenger(Config)
     * @note calls @ref create()
     */
    explicit DebugMessenger(Config const& config, not_null<Instance const*> instance) : DebugMessenger{config} { create(instance); }


    /**
     * @note calls @ref optDestroy()
     */
    ~DebugMessenger() { optDestroy(); }

    /**
     * @brief creates the messenger
     * @param instance must be valid
     * @note may call @ref optDestroy()
     * @throws cth::except::default_exception reason: vkGetInstanceProcAddr() returned nullptr
     * @throws cth::vk::result_exception result of @ref vkCreateDebugUtilsMessengerEXT()
     */
    void create(not_null<Instance const*> instance);


    /**
     * @brief destroys and resets
     * @note @ref created() == true required
     * @throws cth::except::default_exception reason: vkGetInstanceProcAddr() returned nullptr
     */
    void destroy();


    /**
     * @brief if @ref created() == true calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    static void destroy(VkInstance instance, VkDebugUtilsMessengerEXT vk_messenger);

    /**
     * @brief releases ownership, returns state and resets
     * @note @ref created() == true required
     */
    State release();


    struct Config {
        std::function<callback_t> callback = dev::defaultDebugCallback;
        VkDebugUtilsMessageSeverityFlagsEXT messageSeverities = constants::DEBUG_MESSAGE_SEVERITY;
        VkDebugUtilsMessageTypeFlagsEXT messageTypes = constants::DEBUG_MESSAGE_TYPE;


        static Config Default(std::function<callback_t> const& callback = nullptr) {
            return Config{callback == nullptr ? dev::defaultDebugCallback : callback};
        }


        [[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT createInfo() const;
    };

protected:
    Instance const* _instance = nullptr;
    Config _config;

private:
    void reset();

    move_ptr<VkDebugUtilsMessengerEXT_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkDebugUtilsMessengerEXT get() const { return _handle.get(); }
    [[nodiscard]] Config config() const { return _config; }

    DebugMessenger(DebugMessenger const& other) = delete;
    DebugMessenger(DebugMessenger&& other) noexcept = default;
    DebugMessenger& operator=(DebugMessenger const& other) = delete;
    DebugMessenger& operator=(DebugMessenger&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(not_null<DebugMessenger const*> debug_messenger);

#define DEBUG_CHECK_MESSENGER(messenger_ptr) DebugMessenger::debug_check(messenger_ptr)
#else
#define DEBUG_CHECK_MESSENGER(messenger_ptr) ((void)0)
#endif


};
} // namespace cth

namespace cth::vk {
struct DebugMessenger::State {
    not_null<Instance const*> instance;
    gsl::owner<VkDebugUtilsMessengerEXT> vkMessenger; // NOLINT(cppcoreguidelines-owning-memory)
};

}
