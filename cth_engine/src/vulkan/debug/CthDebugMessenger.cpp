#include "CthDebugMessenger.hpp"

#include "src/vulkan/base/CthInstance.hpp"
#include "src/vulkan/utility/cth_vk_exceptions.hpp"



namespace cth::vk {

DebugMessenger::DebugMessenger(Config config): _config{std::move(config)} {}
void DebugMessenger::create(Instance const& instance) {
    Instance::debug_check(instance);


    optDestroy();

    _instance = &instance;


    VkDebugUtilsMessengerCreateInfoEXT const info = _config.createInfo();


    VkDebugUtilsMessengerEXT ptr = VK_NULL_HANDLE;

    VkResult const createResult = vkCreateDebugUtilsMessengerEXT(_instance->get(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to set up debug messenger") {
        reset();
        throw cth::vk::result_exception{createResult, details->exception()};
    }

    _handle = ptr;
}
void DebugMessenger::destroy() {
    DebugMessenger::debug_check(*this);

    destroy(_instance->get(), _handle.get());

    reset();
}


void DebugMessenger::destroy(vk::not_null<VkInstance> vk_instance, VkDebugUtilsMessengerEXT vk_messenger) {
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

namespace cth::vk {
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


namespace cth::dev {
VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type, VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    [[maybe_unused]] void* user_data) {
    except::Severity severity = except::CRITICAL;

    if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) severity = except::LOG;
    else if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) severity = except::INFO;
    else if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) severity = except::WARNING;
    else if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) severity = except::ERR;

    std::string type = "UNKNOWN";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) type = "GENERAL";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) type = "VALIDATION";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) type = "PERFORMANCE";


    cth::log::msg(
        severity,
        "VALIDATION LAYER: {0} {1}:\n   NAME: {2}\n\t (CODE: {3})\n{4}\n\n",
        type,
        to_string(severity),
        callback_data->pMessageIdName,
        callback_data->messageIdNumber,
        callback_data->pMessage
    );


    return VK_FALSE;
}
}
