#include "context.hpp"

#include "cvk/base/queue/queue_family.hpp"

#include <cth/os/osdef.hpp>

#include <cstddef>
#include <cstdlib>


namespace timgui::test {
jvk::Core::Config const Context::CONFIG{
    .vk = {.queueProperties = {cvk::QueueFamilyProperties::GRAPHICS}},
};

namespace {
    size_t errors = 0;

    VKAPI_ATTR VkBool32 VKAPI_CALL count_errors(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT types,
        VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
        void* userData
    ) {
        if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            ++errors;
        return cvk::default_debug_callback(severity, types, callbackData, userData);
    }
}

bool display_available() {
#ifdef CTH_PLATFORM_WINDOWS
    return true;
#else
    return std::getenv("WAYLAND_DISPLAY") != nullptr;
#endif
}

cvk::DebugMessenger::Config counting_messenger() {
    return cvk::DebugMessenger::Config{.callback = count_errors};
}

Context::ValidationCheck::ValidationCheck() { errors = 0; }

Context::ValidationCheck::~ValidationCheck() { EXPECT_EQ(errors, 0); }

Context::Context() :
    instance{instance_config({}, counting_messenger()), cvk::create},
    core{instance, CONFIG, {}} {}
}
