#pragma once
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/ptr.hpp>
#include <cth/io/log.hpp>


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
     */
    explicit DebugMessenger(Config config);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref DebugMessenger(Config)
     * - @ref create()
     */
    explicit DebugMessenger(Config const& config, Instance const& instance) : DebugMessenger{config} {
        create(instance);
    }


    /**
     * @note calls @ref optDestroy()
     */
    ~DebugMessenger() { optDestroy(); }

    /**
     * @brief creates the messenger
     * @pre @ref Instance::created()
     * @pre the instance enables VK_EXT_debug_utils
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateDebugUtilsMessengerEXT()
     */
    void create(Instance const& instance);


    /**
     * @brief destroys and resets
     * @pre @ref created()
     */
    void destroy();


    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    static void destroy(vk_not_null<VkInstance> vkInstance, VkDebugUtilsMessengerEXT vkMessenger);

    /**
     * @brief releases ownership, returns state and resets
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

    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkDebugUtilsMessengerEXT get() const { return _handle.get(); }
    [[nodiscard]] Config config() const { return _config; }

protected:
    /**
     * @brief handle, not the wrapper: the instance can move
     */
    VkInstance _vkInstance = VK_NULL_HANDLE;
    Config _config;

private:
    void reset();

    move_ptr<VkDebugUtilsMessengerEXT_T> _handle = VK_NULL_HANDLE;

public:
    DebugMessenger(DebugMessenger const& other) = delete;
    DebugMessenger(DebugMessenger&& other) noexcept = default;
    DebugMessenger& operator=(DebugMessenger const& other) = delete;
    DebugMessenger& operator=(DebugMessenger&& other) noexcept = default;

    static void debug_check(DebugMessenger const& debugMessenger);
};
} // namespace cvk

namespace cvk {
struct DebugMessenger::State {
    vk_not_null<VkInstance> vkInstance;
    vk_not_null<VkDebugUtilsMessengerEXT> vkMessenger;
};
}

namespace cvk {
inline void DebugMessenger::debug_check(DebugMessenger const& debugMessenger) {
    CTH_CRITICAL(!debugMessenger.created(), "debug_messenger not created") {}
}
}
