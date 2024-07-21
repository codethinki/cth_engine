#include "CthBasicDebugMessenger.hpp"

#include "vulkan/base/CthInstance.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {

void BasicDebugMessenger::create(BasicInstance const* instance) {
    _instance = instance;

    DEBUG_CHECK_INSTANCE(instance);
    DEBUG_CHECK_MESSENGER_LEAK(this);

    VkDebugUtilsMessengerCreateInfoEXT const info = _config.createInfo();

    auto const func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(_instance->get(), "vkCreateDebugUtilsMessengerEXT"));

    CTH_STABLE_ERR(func == nullptr, "vkGetInstanceProcAddr returned nullptr") throw details->exception();

    VkDebugUtilsMessengerEXT ptr = VK_NULL_HANDLE;

    VkResult const createResult = func(_instance->get(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to set up debug messenger")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    _handle = ptr;
}
void BasicDebugMessenger::destroy() {

    destroy(_instance->get(), _handle.get());

    _handle = VK_NULL_HANDLE;
    _instance = nullptr;
}

void BasicDebugMessenger::destroy(VkInstance instance, VkDebugUtilsMessengerEXT vk_messenger) {
    CTH_WARN(vk_messenger == VK_NULL_HANDLE, "messenger invalid") {}
    DEBUG_CHECK_INSTANCE_HANDLE(instance);

    auto const func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

    CTH_STABLE_ERR(func == nullptr, "vkGetInstanceProcAddr returned nullptr") throw details->exception();

    func(instance, vk_messenger, nullptr);
}
#ifdef CONSTANT_DEBUG_MODE
void BasicDebugMessenger::debug_check(BasicDebugMessenger const* debug_messenger) {
    CTH_ERR(debug_messenger == nullptr, "debug_messenger must not be nullptr") throw details->exception();
    CTH_ERR(debug_messenger->get() == VK_NULL_HANDLE, "debug_messenger invalid") throw details->exception();
}
void BasicDebugMessenger::debug_check_leak(BasicDebugMessenger const* debug_messenger) {
    CTH_WARN(debug_messenger->_handle != VK_NULL_HANDLE, "debug_messenger replaced (potential memory leak)") {}
}
#endif
} // namespace cth

//Config

namespace cth::vk {
VkDebugUtilsMessengerCreateInfoEXT BasicDebugMessenger::Config::createInfo() const {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    createInfo.pfnUserCallback = *callback.target<callback_t*>();
    createInfo.pUserData = nullptr; // Optional

    return createInfo;

}
}


namespace cth::dev {
VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT const message_severity,
    VkDebugUtilsMessageTypeFlagsEXT const message_type, VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    void* user_data) {
    except::Severity severity = except::CRITICAL;

    if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) severity = except::LOG;
    else if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) severity = except::INFO;
    else if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) severity = except::WARNING;
    else if(message_severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) severity = except::ERR;

    std::string type = "UNKNOWN";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) type = "GENERAL";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) type = "VALIDATION";
    if(message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) type = "PERFORMANCE";


    cth::log::msg(severity, "VALIDATION LAYER: {0} {1}:\n   NAME: {2}\n\t (CODE: {3})\n{4}", type, except::to_string(severity),
        callback_data->pMessageIdName,
        callback_data->messageIdNumber, callback_data->pMessage);


    return VK_FALSE;
}
}
