#include "jvk/debug/debug_messenger.hpp"
#include "jvk/base/instance.hpp"
#include "jvk/utility/vk_exceptions.hpp"

#include <cth/data/string_joiner.hpp>

namespace jvk {

DebugMessenger::DebugMessenger(Config config) : _config{std::move(config)} {}

void DebugMessenger::create(Instance const& instance) {
    Instance::debug_check(instance);


    optDestroy();

    _instance = &instance;


    auto const info = _config.createInfo();


    VkDebugUtilsMessengerEXT ptr = VK_NULL_HANDLE;

    auto const createResult = vkCreateDebugUtilsMessengerEXT(_instance->get(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to set up debug messenger") {
        reset();
        throw jvk::vk_result_exception{createResult, details->exception()};
    }

    _handle = ptr;
}

void DebugMessenger::destroy() {
    DebugMessenger::debug_check(*this);

    destroy(_instance->get(), _handle.get());

    reset();
}


void DebugMessenger::destroy(
    jvk::vk_not_null<VkInstance> vk_instance,
    VkDebugUtilsMessengerEXT vk_messenger
) {
    Instance::debug_check_handle(vk_instance.get());
    CTH_WARN(vk_messenger == VK_NULL_HANDLE, "messenger invalid") {}

    vkDestroyDebugUtilsMessengerEXT(vk_instance.get(), vk_messenger, nullptr);
}

DebugMessenger::State DebugMessenger::release() {
    State const state{_instance, _handle.get()};
    reset();
    return state;
}

} // namespace cth

//Config
namespace jvk::dev {
VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    void* user_data
);
}

namespace jvk {
DebugMessenger::Config DebugMessenger::Config::Default(std::function<callback_t> const& callback) {
    return Config{callback == nullptr ? dev::default_debug_callback : callback};
}

std::function<DebugMessenger::callback_t> DebugMessenger::Config::defaultCallback() {
    return dev::default_debug_callback;
}

VkDebugUtilsMessengerCreateInfoEXT DebugMessenger::Config::createInfo() const {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    createInfo.messageSeverity = messageSeverities;

    createInfo.messageType = messageTypes;

    createInfo.pfnUserCallback = *callback.target<callback_t*>();
    createInfo.pUserData = nullptr; // Optional

    return createInfo;
}

void DebugMessenger::reset() {
    _instance = nullptr;
    _handle = VK_NULL_HANDLE;
}


}


namespace jvk::dev {
struct component_info {
    std::string name;
    VkObjectType objectType;
    uint64_t handle;

    static auto to_string(component_info const& info) {
        return std::format("name: {0}, type: {1}, handle: {2:#x}", info.name, info.objectType, info.handle);
    }
};
}

CTH_FORMAT_CLASS_ATTRIBUTES(
    jvk::dev::component_info,
    "name: {0}, type: {1}, handle: {2:#x}",
    [](jvk::dev::component_info const& c) { return std::tie(c.name, c.objectType, c.handle); }
);


namespace jvk::dev {

namespace {
    auto to_severity(VkDebugUtilsMessageSeverityFlagsEXT vk_severity) {
        using vk_severity_t = VkDebugUtilsMessageSeverityFlagBitsEXT;

        cxpr std::array severities{
            std::pair{VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, cth::except::LOG},
            std::pair{VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, cth::except::INFO},
            std::pair{VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, cth::except::WARNING},
            std::pair{VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, cth::except::ERR}
        };


        for(auto [vkSeverityFlagBits, severity] : severities)
            if(vk_severity <= vkSeverityFlagBits)
                return severity;

        return cth::except::CRITICAL;
    }

    std::string to_msg(VkDebugUtilsMessageTypeFlagBitsEXT vk_msg_type) {
        cxpr std::array msgTypes{
            std::pair{VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, "GENERAL"},
            std::pair{VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, "VALIDATION"},
            std::pair{VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, "PERFORMANCE"}
        };

        cth::dt::string_joiner joiner{" | "};

        for(auto [vkSeverityFlagBit, name] : msgTypes)
            if(vk_msg_type & vkSeverityFlagBit)
                joiner += name;

        return joiner;
    }

}

VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    [[maybe_unused]] void* user_data
) {
    auto const severity = to_severity(message_severity);


    std::string type = "UNKNOWN";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) type = "GENERAL";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) type = "VALIDATION";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) type = "PERFORMANCE";


    size_t const objectCount = callback_data->objectCount;
    std::vector<component_info> objects{};
    objects.reserve(objectCount);

    for(size_t i = 0; i < objectCount; i++) {
        auto const& object = callback_data->pObjects[i];
        auto namePtr = object.pObjectName;
        objects.emplace_back(
            namePtr == nullptr ? "UNKNOWN" : namePtr,
            object.objectType,
            object.objectHandle
        );
    }

    cth::log::msg(
        severity,
        "VALIDATION LAYER: {0} {1}:\n   NAME: {2}\n\t (CODE: {3})\n{4}\n   OBJECTS: {5}\n",
        type,
        to_string(severity),
        callback_data->pMessageIdName,
        callback_data->messageIdNumber,
        callback_data->pMessage,
        objects
    );


    return VK_FALSE;
}
}
