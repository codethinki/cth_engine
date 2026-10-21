#pragma once
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/instance_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/ptr.hpp>


namespace cvk {
class Instance;

/**
 * @brief writes the message to cth::log
 * @return VK_FALSE, the call that caused the message continues
 */
VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    VkDebugUtilsMessengerCallbackDataEXT const*,
    void* userData
);

/**
 * @brief VkDebugUtilsMessengerEXT wrapper
 */
class DebugMessenger {
public:
    using vk_handle_t = VkDebugUtilsMessengerEXT;
    struct Config;
    struct State;
    using callback_t = PFN_vkDebugUtilsMessengerCallbackEXT;

    /**
     * @brief base constructor
     * @pre @ref Instance::created()
     */
    DebugMessenger(Instance const&, Config);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref DebugMessenger(Instance const&, Config)
     * - @ref wrap()
     */
    DebugMessenger(Instance const&, Config, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref DebugMessenger(Instance const&, Config)
     * - @ref create()
     */
    explicit DebugMessenger(Instance const&, Config, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~DebugMessenger() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the instance created the messenger
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the messenger
     * @pre the instance enables VK_EXT_debug_utils
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateDebugUtilsMessengerEXT()
     */
    void create();

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    static void destroy(vk_not_null<VkInstance>, VkDebugUtilsMessengerEXT vkMessenger);

    /**
     * @brief releases ownership, returns state and forgets
     * @pre @ref created()
     */
    State release();

    struct Config {
        /**
         * @pre != nullptr
         */
        callback_t callback = &default_debug_callback;
        VkDebugUtilsMessageSeverityFlagsEXT messageSeverities = constants::DEBUG_MESSAGE_SEVERITY;
        VkDebugUtilsMessageTypeFlagsEXT messageTypes = constants::DEBUG_MESSAGE_TYPE;

        [[nodiscard]] constexpr VkDebugUtilsMessengerCreateInfoEXT createInfo() const {
            return VkDebugUtilsMessengerCreateInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .pNext = nullptr,
                .flags = 0,
                .messageSeverity = messageSeverities,
                .messageType = messageTypes,
                .pfnUserCallback = callback,
                .pUserData = nullptr,
            };
        }

        static constexpr void debug_check(Config const&);
    };

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Config config() const { return _config; }

private:
    void forget();

    Config _config;
    instance_handle<DebugMessenger> _handle;

public:
    DebugMessenger(DebugMessenger const& other) = delete;
    DebugMessenger(DebugMessenger&& other) noexcept = default;
    DebugMessenger& operator=(DebugMessenger const& other) = delete;
    DebugMessenger& operator=(DebugMessenger&& other) noexcept = default;

    static constexpr void debug_check(DebugMessenger const&);
};
}

namespace cvk {
struct DebugMessenger::State {
    vk_not_null<VkDebugUtilsMessengerEXT> vkMessenger;
};
}

namespace cvk {
constexpr void DebugMessenger::debug_check(DebugMessenger const& debugMessenger) {
    CTH_CRITICAL(!debugMessenger.created(), "debug_messenger not created") {}
}

constexpr void DebugMessenger::Config::debug_check(Config const& config) {
    CTH_CRITICAL(config.callback == nullptr, "callback must not be nullptr") {}
}
}
