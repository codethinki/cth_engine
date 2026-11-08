#include "context.hpp"

#include "cvk/render/sync/fence.hpp"

#include <cstddef>
#include <optional>


namespace jvk::test {
Core::Config const Context::CONFIG{
    .vk = {.queueProperties = {std::from_range, QUEUES}},
    .workers = 2,
    .queueWorkers = 2,
};

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
        cvk::Instance instance{instance_config({}, counting_messenger()), cvk::create};
        std::optional<Core> core{};
    };

    std::optional<SharedContexts> shared{};

    SharedContexts& shared_contexts() {
        if(!shared.has_value()) shared.emplace();
        return *shared;
    }

    Core const& shared_core() {
        auto& contexts = shared_contexts();
        if(!contexts.core.has_value()) contexts.core.emplace(contexts.instance, Context::CONFIG, cvk::create);
        return *contexts.core;
    }
}

cvk::DebugMessenger::Config counting_messenger() { return cvk::DebugMessenger::Config{.callback = count_errors}; }

cvk::queue_family_index_t queue_family(Context const& context) { return context.core.queue(0).vk().familyIndex(); }

void submit_and_wait(Context const& context, cvk::PrimaryCmdBuffer const& primary) {
    cvk::Fence const fence{context.core, cvk::create};

    context.core.queue(0).vk().submit({.cmdBuffers = {primary}}, &fence);
    fence.wait();
}

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
