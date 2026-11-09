#include "context.hpp"

#include "cvk/render/sync/fence.hpp"
#include "cvk/utility/constant/device_constants.hpp"

#include <cstddef>
#include <optional>
#include <ranges>


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
        cvk::Instance instance{instance_config(headless_extensions(), counting_messenger()), cvk::create};
        std::optional<Core> core{};
    };

    std::optional<SharedContexts> shared{};

    SharedContexts& shared_contexts() {
        if(!shared.has_value()) shared.emplace();
        return *shared;
    }

    Core const& shared_core() {
        auto& contexts = shared_contexts();
        if(!contexts.core.has_value()) contexts.core.emplace(contexts.instance, Context::CONFIG, NO_SURFACES);
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

std::vector<std::string> headless_extensions() {
    std::vector<std::string> extensions{
        std::from_range,
        cvk::constants::PRESENT_INSTANCE_EXTENSIONS | std::views::transform(
            [](std::string_view extension) { return std::string{extension}; }
        )
    };
    extensions.emplace_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
    return extensions;
}

cvk::Surface headless_surface(cvk::Instance const& instance) {
    VkHeadlessSurfaceCreateInfoEXT const createInfo{
        .sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
    };

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    auto const result = vkCreateHeadlessSurfaceEXT(instance.get(), &createInfo, nullptr, &vkSurface);
    CTH_STABLE_THROW(result != VK_SUCCESS, "failed to create headless surface") {}

    return cvk::Surface{instance, cvk::Surface::State{vkSurface}};
}

Context::ValidationCheck::ValidationCheck() { errors = 0; }

Context::ValidationCheck::~ValidationCheck() { EXPECT_EQ(errors, 0); }

void destroy_shared_contexts() {
    if(!shared.has_value()) return;

    errors = 0;
    shared.reset();
    EXPECT_EQ(errors, 0) << "errors at the destruction of the shared contexts, e.g. leaks";
}
}
