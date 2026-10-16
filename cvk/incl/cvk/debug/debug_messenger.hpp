#pragma once
#include "cvk/utility/constants.hpp"
#include "cvk/utility/instance_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/ptr.hpp>


namespace cvk {
class Instance;

/**
 * @brief VkDebugUtilsMessengerEXT wrapper
 */
class DebugMessenger {
public:
    struct Config;
    struct State;
    using callback_t = PFN_vkDebugUtilsMessengerCallbackEXT;


    /**
     * @brief base constructor
     * @pre @ref Instance::created()
     */
    DebugMessenger(Instance const&, Config);

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
        callback_t callback = defaultCallback();
        VkDebugUtilsMessageSeverityFlagsEXT messageSeverities = constants::DEBUG_MESSAGE_SEVERITY;
        VkDebugUtilsMessageTypeFlagsEXT messageTypes = constants::DEBUG_MESSAGE_TYPE;

        /**
         * @param callback nullptr -> @ref defaultCallback()
         */
        static Config Default(callback_t callback = nullptr);

        /**
         * @brief writes the message to cth::log
         */
        static callback_t defaultCallback();

        [[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT createInfo() const;
    };

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkDebugUtilsMessengerEXT get() const { return _handle.get(); }
    [[nodiscard]] constexpr Config config() const { return _config; }

protected:
    Config _config;

private:
    void forget();

    instance_handle<VkDebugUtilsMessengerEXT, DebugMessenger> _handle;

public:
    DebugMessenger(DebugMessenger const& other) = delete;
    DebugMessenger(DebugMessenger&& other) noexcept = default;
    DebugMessenger& operator=(DebugMessenger const& other) = delete;
    DebugMessenger& operator=(DebugMessenger&& other) noexcept = default;

    static constexpr void debug_check(DebugMessenger const&);
};
} // namespace cvk

namespace cvk {
struct DebugMessenger::State {
    vk_not_null<VkDebugUtilsMessengerEXT> vkMessenger;
};
}

namespace cvk {
constexpr void DebugMessenger::debug_check(DebugMessenger const& debugMessenger) {
    CTH_CRITICAL(!debugMessenger.created(), "debug_messenger not created") {}
}
}
