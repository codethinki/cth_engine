#include "CthInstance.hpp"

#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/utility/CthConstants.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#ifdef CONSTANT_DEBUG_MODE
#include "../debug/CthBasicDebugMessenger.hpp"
#endif

#include <cth/cth_log.hpp>

#include <vector>

namespace cth {

using std::string;
using std::string_view;
using std::vector;


BasicInstance::BasicInstance(std::string app_name, const std::vector<std::string>& required_extensions) : _name(std::move(app_name)),
    _requiredExt{required_extensions}, _availableExt(getAvailableInstanceExtensions()) { checkInstanceExtensionSupport(); }

void BasicInstance::wrap(VkInstance vk_instance) {
    DEBUG_CHECK_INSTANCE_LEAK(this);

    _handle = vk_instance;
}

void BasicInstance::create(const std::optional<BasicDebugMessenger::Config>& messenger_config) {
    DEBUG_CHECK_INSTANCE_LEAK(this);

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    const auto appInfo = this->appInfo();
    createInfo.pApplicationInfo = &appInfo;

    const auto requiredExtVec = utils::toCharVec(_requiredExt);
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtVec.size());
    createInfo.ppEnabledExtensionNames = requiredExtVec.data();
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;

    if constexpr(Constant::ENABLE_VALIDATION_LAYERS)
        if(messenger_config != std::nullopt) {
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

            createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

            debugCreateInfo = messenger_config->createInfo();
            createInfo.pNext = &debugCreateInfo;
        }

    VkInstance ptr = VK_NULL_HANDLE;
    const VkResult createInstanceResult = vkCreateInstance(&createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createInstanceResult != VK_SUCCESS, "failed to create instance!")
        throw cth::except::vk_result_exception{createInstanceResult, details->exception()};


    _handle = ptr;
}
void BasicInstance::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) deletion_queue->push(_handle.get());
    else vkDestroyInstance(_handle.get(), nullptr);
}


void BasicInstance::checkInstanceExtensionSupport() {
    vector<string> missingExtensions{};

    std::ranges::for_each(_requiredExt, [&](const string_view extension) {
        if(!std::ranges::contains(_availableExt, extension)) missingExtensions.emplace_back(extension);
    });
    CTH_STABLE_ERR(!missingExtensions.empty(), "instance extensions missing") {
        std::ranges::for_each(missingExtensions, [&details](const string_view extension) { details->add(extension); });

        throw details->exception();
    }
}
void BasicInstance::checkValidationLayerSupport() {
    if constexpr(Constant::ENABLE_VALIDATION_LAYERS) {
        vector<string> missingLayers{};

        std::ranges::for_each(VALIDATION_LAYERS, [&](const string_view layer) {
            if(!std::ranges::contains(_availableLayers, layer)) missingLayers.emplace_back(layer);
        });
        CTH_STABLE_ERR(!missingLayers.empty(), "validation layers missing") {
            std::ranges::for_each(missingLayers, [&details](const string_view layer) { details->add(layer); });

            throw details->exception();
        }
    }
}
vector<string> BasicInstance::getAvailableValidationLayers() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    vector<string> layers(availableLayers.size());
    std::ranges::transform(availableLayers, layers.begin(), [](const VkLayerProperties& layer) { return string(layer.layerName); });

    return layers;
}

vector<string> BasicInstance::getAvailableInstanceExtensions() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    vector<string> availableExtensionsStr{availableExtensions.size()};
    std::ranges::transform(availableExtensions, availableExtensionsStr.begin(),
        [](const VkExtensionProperties& ext) { return ext.extensionName; });


    return availableExtensionsStr;
}
VkApplicationInfo BasicInstance::appInfo() const {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = _name.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = _name.c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(1, 2, 0);
    appInfo.apiVersion = VK_MAKE_VERSION(1, 2, 0);
    return appInfo;
}
void BasicInstance::destroy(VkInstance vk_instance) {
    CTH_WARN(vk_instance == nullptr, "vk_instance invalid");

    vkDestroyInstance(vk_instance, nullptr);

    cth::log::msg<except::LOG>("destroyed instance");
}

#ifdef CONSTANT_DEBUG_MODE
void BasicInstance::debug_check(const BasicInstance* instance) {
    CTH_ERR(instance == nullptr, "instance must not be nullptr") throw details->exception();
    CTH_ERR(instance->_handle == VK_NULL_HANDLE, "instance must be a valid handle") throw details->exception();
}
void BasicInstance::debug_check_leak(const BasicInstance* instance) {
    CTH_WARN(instance->_handle != VK_NULL_HANDLE, "instance replaced (potential memory leak)");
}
#endif


} // namespace cth


namespace cth {
using namespace std;

Instance::Instance(string app_name, const vector<string>& required_extensions) : BasicInstance(std::move(app_name), required_extensions) {
    if constexpr(Constant::ENABLE_VALIDATION_LAYERS) {
        _availableLayers = getAvailableValidationLayers();
        checkValidationLayerSupport();

        _requiredExt.insert(_requiredExt.begin(), VALIDATION_LAYER_EXTENSIONS.begin(), VALIDATION_LAYER_EXTENSIONS.end());
    }
    Instance::create(std::nullopt);
}
Instance::~Instance() {
#ifdef CONSTANT_DEBUG_MODE
    _debugMessenger = nullptr;
#endif

    if(get() != VK_NULL_HANDLE) destroy();
}



void Instance::wrap(VkInstance vk_instance) {
    if(get() != VK_NULL_HANDLE) destroy();

    BasicInstance::wrap(vk_instance);
}
void Instance::create(const std::optional<BasicDebugMessenger::Config>& messenger_config) {
    if(get() != VK_NULL_HANDLE) destroy();


#ifndef CONSTANT_DEBUG_MODE
    BasicInstance::create(messenger_config);

#else
    if(messenger_config != std::nullopt) {
        BasicInstance::create(messenger_config);
        return;
    }
    BasicDebugMessenger::Config config;

    config = BasicDebugMessenger::Config::Default();
    BasicInstance::create(config);
    _debugMessenger = make_unique<DebugMessenger>(nullptr, this, config);
#endif
}



} // namespace cth
