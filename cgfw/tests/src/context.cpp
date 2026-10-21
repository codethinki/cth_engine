#include "context.hpp"

#include <cth/os/osdef.hpp>

#include <cstddef>
#include <cstdlib>


namespace cgfw::test {
namespace {
    size_t errors = 0;

    VKAPI_ATTR VkBool32 VKAPI_CALL count_errors(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT types,
        VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
        void* userData
    ) {
        if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++errors;
        return cvk::default_debug_callback(severity, types, callbackData, userData);
    }
}

bool display_available() {
#ifdef CTH_PLATFORM_WINDOWS
    return true;
#else
    //TODO a headless weston in the CI container, the tests skip there
    return std::getenv("WAYLAND_DISPLAY") != nullptr;
#endif
}

cvk::DebugMessenger::Config counting_messenger() { return cvk::DebugMessenger::Config{.callback = count_errors}; }

ValidationCheck::ValidationCheck() { errors = 0; }

ValidationCheck::~ValidationCheck() { EXPECT_EQ(errors, 0); }

InstanceContext::InstanceContext() : instance{instance_config(instance_extensions()), cvk::create, counting_messenger()} {}
}
