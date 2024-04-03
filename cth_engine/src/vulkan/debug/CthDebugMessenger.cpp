#include "CthDebugMessenger.hpp"

#include <vulkan/base/CthInstance.hpp>
#include <vulkan/utility/CthVkUtils.hpp>

#include <cth/cth_log.hpp>

namespace cth {

DebugMessenger::DebugMessenger(const function<callback_t>& custom_callback) { setCallback(custom_callback); }
DebugMessenger::DebugMessenger(Instance* instance, const function<callback_t>& custom_callback) {
    setCallback(custom_callback);
    init(instance);
}

DebugMessenger::~DebugMessenger() { destroy(); }

void DebugMessenger::init(Instance* instance) {
    CTH_ERR(this->vkMessenger != VK_NULL_HANDLE, "double initialization is not allowed") throw details->exception();
    this->instance = instance;


    const VkDebugUtilsMessengerCreateInfoEXT info = createInfo();

    const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance->get(), "vkCreateDebugUtilsMessengerEXT"));

    CTH_STABLE_ERR(func == nullptr, "vkGetInstanceProcAddr returned nullptr") throw details->exception();

    const VkResult createResult = func(instance->get(), &info, nullptr, &vkMessenger);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to set up debug messenger")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}
void DebugMessenger::destroy() {
    CTH_WARN(vkMessenger == VK_NULL_HANDLE, "debug messenger already inactive");
    if(vkMessenger == VK_NULL_HANDLE) return;

    const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance->get(), "vkDestroyDebugUtilsMessengerEXT"));

    if(func != nullptr) func(instance->get(), vkMessenger, nullptr);
    vkMessenger = VK_NULL_HANDLE;
    instance = nullptr;
}


void DebugMessenger::setCallback(const function<callback_t>& custom_callback) {
    callback = custom_callback == nullptr ? cth::dev::defaultDebugCallback : custom_callback;
}
VkDebugUtilsMessengerCreateInfoEXT DebugMessenger::createInfo() const {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    createInfo.pfnUserCallback = *callback.target<callback_t*>();
    createInfo.pUserData = nullptr; // Optional

    return createInfo;

}

} // namespace cth

namespace cth::dev {
VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    const VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {

    except::Severity severity = except::CRITICAL;

    if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) severity = except::LOG;
    else if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) severity = except::INFO;
    else if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) severity = except::WARNING;
    else if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) severity = except::ERR;

    string type = "UNKNOWN";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) type = "GENERAL";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) type = "VALIDATION";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) type = "PERFORMANCE";


    cth::log::msg(severity, "VALIDATION LAYER: {0} {1}:\n   NAME: {2}\n\t (CODE: {3})\n{4}", type, except::to_string(severity),
        callback_data->pMessageIdName,
        callback_data->messageIdNumber, callback_data->pMessage);


    return VK_FALSE;
}
}
