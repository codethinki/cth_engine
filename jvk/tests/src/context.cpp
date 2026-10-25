#include "context.hpp"

#include "cvk/surface/surface.hpp"

#include <cstddef>
#include <optional>
#include <span>


namespace jvk::test {
cvk::Core::Config const Context::CONFIG{.queueProperties = {std::from_range, QUEUES}};

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

    /**
     * one instance and core for all tests, the core comes at its first use
     */
    struct SharedContexts {
        cvk::Instance instance{instance_config(), cvk::create, counting_messenger()};
        std::optional<cvk::Core> core{};
    };

    std::optional<SharedContexts> shared{};

    SharedContexts& shared_contexts() {
        if(!shared.has_value()) shared.emplace();
        return *shared;
    }

    cvk::Core const& shared_core() {
        auto& contexts = shared_contexts();
        if(!contexts.core.has_value())
            contexts.core.emplace(contexts.instance, std::span<cvk::Surface const>{}, Context::CONFIG);
        return *contexts.core;
    }
}

cvk::DebugMessenger::Config counting_messenger() { return cvk::DebugMessenger::Config{.callback = count_errors}; }

Context::Context() : instance{shared_contexts().instance}, core{shared_core()} {}

Context::ValidationCheck::ValidationCheck() { errors = 0; }

Context::ValidationCheck::~ValidationCheck() { EXPECT_EQ(errors, 0); }

void destroy_shared_contexts() {
    if(!shared.has_value()) return;

    errors = 0;
    shared.reset();
    EXPECT_EQ(errors, 0) << "errors at the destruction of the shared contexts, e.g. leaks";
}
}
