#include "cvk/debug/debug_messenger.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/data/string_joiner.hpp>

#include <span>


namespace cvk {
DebugMessenger::DebugMessenger(Instance const& instance, Config config) : _config{std::move(config)},
    _handle{instance.get()} { Instance::debug_check(instance); }

DebugMessenger::DebugMessenger(Instance const& instance, Config config, State const& state) : DebugMessenger{
    instance,
    std::move(config)
} { wrap(state); }

DebugMessenger::DebugMessenger(Instance const& instance, Config config, create_t) : DebugMessenger{
    instance,
    std::move(config)
} { create(); }

void DebugMessenger::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkMessenger.get(), _handle.vkInstance());
}

void DebugMessenger::create() {
    optDestroy();

    auto const info = _config.createInfo();

    VkDebugUtilsMessengerEXT ptr = VK_NULL_HANDLE;
    auto const createResult = vkCreateDebugUtilsMessengerEXT(_handle.vkInstance(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to set up debug messenger") {
        forget();
        throw cvk::vk_result_exception{createResult, details->exception()};
    }

    _handle.reset(ptr, _handle.vkInstance());
}

void DebugMessenger::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}


void DebugMessenger::destroy(vk_not_null<VkInstance> vkInstance, VkDebugUtilsMessengerEXT vkMessenger) {
    Instance::debug_check_handle(vkInstance.get());
    CTH_WARN(vkMessenger == VK_NULL_HANDLE, "messenger invalid") {}

    vkDestroyDebugUtilsMessengerEXT(vkInstance.get(), vkMessenger, nullptr);
}

DebugMessenger::State DebugMessenger::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

void DebugMessenger::forget() {}
} // namespace cvk

namespace cvk::dev {
VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
    void* userData
);
}

namespace cvk {
DebugMessenger::Config DebugMessenger::Config::Default(callback_t callback) {
    return Config{callback == nullptr ? defaultCallback() : callback};
}

DebugMessenger::callback_t DebugMessenger::Config::defaultCallback() { return dev::default_debug_callback; }

VkDebugUtilsMessengerCreateInfoEXT DebugMessenger::Config::createInfo() const {
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
}


namespace cvk::dev {
struct component_info {
    std::string name;
    VkObjectType objectType;
    uint64_t handle;
};
}

CTH_FORMAT_CLASS_CUSTOM(
    cvk::dev::component_info,
    "name: {0}, type: {1}, handle: {2:#x}",
    [](cvk::dev::component_info const& c) { return std::tie(c.name, c.objectType, c.handle); }
);


namespace cvk::dev {
namespace {
    auto to_severity(VkDebugUtilsMessageSeverityFlagsEXT vkSeverity) {
        constexpr std::array severities{
            std::pair{VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, cth::except::LOG},
            std::pair{VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, cth::except::INFO},
            std::pair{VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, cth::except::WARNING},
            std::pair{VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, cth::except::ERR}
        };


        for(auto [vkSeverityFlagBits, severity] : severities)
            if(vkSeverity <= vkSeverityFlagBits)
                return severity;

        return cth::except::CRITICAL;
    }

    std::string to_msg(VkDebugUtilsMessageTypeFlagsEXT vkMsgType) {
        constexpr std::array msgTypes{
            std::pair{VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, "GENERAL"},
            std::pair{VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, "VALIDATION"},
            std::pair{VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, "PERFORMANCE"}
        };

        cth::dt::string_joiner joiner{" | "};

        for(auto [vkMsgTypeFlagBit, name] : msgTypes)
            if(vkMsgType & vkMsgTypeFlagBit)
                joiner += name;

        return joiner;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
    [[maybe_unused]] void* userData
) {
    auto const severity = to_severity(messageSeverity);

    auto type = to_msg(messageType);
    if(type.empty()) type = "UNKNOWN";


    std::vector<component_info> objects{};
    objects.reserve(callbackData->objectCount);

    for(auto const& object : std::span{callbackData->pObjects, callbackData->objectCount}) {
        auto const namePtr = object.pObjectName;
        objects.emplace_back(
            namePtr == nullptr ? "UNKNOWN" : namePtr,
            object.objectType,
            object.objectHandle
        );
    }

    auto const idNamePtr = callbackData->pMessageIdName;

    cth::log::msg(
        severity,
        "VALIDATION LAYER: {0} {1}:\n   NAME: {2}\n\t (CODE: {3})\n{4}\n   OBJECTS: {5}\n",
        type,
        to_string(severity),
        idNamePtr == nullptr ? "UNKNOWN" : idNamePtr,
        callbackData->messageIdNumber,
        callbackData->pMessage,
        objects
    );


    return VK_FALSE;
}
}
