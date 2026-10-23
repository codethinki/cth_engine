#include "validation.hpp"


namespace cvk::test {
namespace {
    size_t errors = 0;

    VKAPI_ATTR VkBool32 VKAPI_CALL count_errors(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT types,
        VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
        void* userData
    ) {
        if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++errors;
        return default_debug_callback(severity, types, callbackData, userData);
    }
}

DebugMessenger::Config counting_messenger() { return DebugMessenger::Config{.callback = count_errors}; }

size_t validation_errors() { return errors; }

void reset_validation_errors() { errors = 0; }
}
