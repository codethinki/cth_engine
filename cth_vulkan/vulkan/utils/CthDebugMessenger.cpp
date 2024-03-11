#include "CthDebugMessenger.hpp"

#include "cth_vk_specific_utils.hpp"
#include "../core/CthInstance.hpp"

#include <cth/cth_log.hpp>

namespace cth {

DebugMessenger::DebugMessenger(const function<callback_t>& custom_callback) { setCallback(custom_callback); }
DebugMessenger::DebugMessenger(VkInstance instance, const function<callback_t>& callback) {
    setCallback(callback);
    init(instance);
}
DebugMessenger::~DebugMessenger() { if(active) destroy<false>(vkInstance); }

void DebugMessenger::init(VkInstance instance) {
    this->vkInstance = instance;

    CTH_STABLE_ERR(active, "double initialization is not allowed") throw details->exception();
    active = true;

    const VkDebugUtilsMessengerCreateInfoEXT info = createInfo();

    const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

    CTH_STABLE_ERR(func == nullptr, "vkGetInstanceProcAddr returned nullptr") throw details->exception();

    const VkResult createResult = func(instance, &info, nullptr, &vkMessenger);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to set up debug messenger")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}
template<bool Throw = true>
void DebugMessenger::destroy(VkInstance instance) const {
    if constexpr(Throw)
        CTH_STABLE_ERR(!active, "cannot destroy uninitialized messenger") throw details->exception();
    if(!active) return;

    const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

    if(func != nullptr) func(instance, vkMessenger, nullptr);

    else if constexpr(Throw)
        CTH_STABLE_ERR(true, "vkGetInstanceProcAddr result was nullptr") throw details->exception();

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


    cth::log::msg(severity, "{0} VALIDATION LAYER {1}:\n CODE: {2}\n{3}", message_type, except::to_string(severity),
        callback_data->messageIdNumber, callback_data->pMessage);


    return VK_FALSE;
}
}
