#include "CthInstance.hpp"

#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"
#include "vulkan/utility/cth_constants.hpp"

#ifdef CONSTANT_DEBUG_MODE
#include "../debug/CthBasicDebugMessenger.hpp"
#endif


namespace cth::vk {

using std::string;
using std::string_view;
using std::vector;
using std::span;


BasicInstance::BasicInstance(string_view const app_name, span<string const> const required_extensions) : _name(app_name),
    _availableExt(getAvailableInstanceExtensions()) {
    _requiredExt.reserve(required_extensions.size() + REQUIRED_INSTANCE_EXTENSIONS.size());
    _requiredExt.insert(_requiredExt.end(), required_extensions.begin(), required_extensions.end());
    _requiredExt.insert(_requiredExt.end(), REQUIRED_INSTANCE_EXTENSIONS.begin(), REQUIRED_INSTANCE_EXTENSIONS.end());

    checkInstanceExtensionSupport();
}



void BasicInstance::wrap(VkInstance vk_instance) {
    DEBUG_CHECK_INSTANCE_LEAK(this);

    _handle = vk_instance;
}

void BasicInstance::create(std::optional<BasicDebugMessenger::Config> const& messenger_config) {
    DEBUG_CHECK_INSTANCE_LEAK(this);

    vector<char const*> requiredExtVec(_requiredExt.size());
    std::ranges::copy(_requiredExt | std::views::transform([](auto const& str) { return str.data(); }), requiredExtVec.begin());

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    auto const appInfo = this->appInfo();
    createInfo.pApplicationInfo = &appInfo;


    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtVec.size());
    createInfo.ppEnabledExtensionNames = requiredExtVec.data();
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;

    if constexpr(constants::ENABLE_VALIDATION_LAYERS)
        if(messenger_config != std::nullopt) {
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

            createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

            debugCreateInfo = messenger_config->createInfo();
            createInfo.pNext = &debugCreateInfo;
        }

    VkInstance ptr = VK_NULL_HANDLE;
    VkResult const createInstanceResult = vkCreateInstance(&createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createInstanceResult != VK_SUCCESS, "failed to create instance!")
        throw cth::except::vk_result_exception{createInstanceResult, details->exception()};


    _handle = ptr;
}
void BasicInstance::destroy() {
    destroy(_handle.get());

    _handle = VK_NULL_HANDLE;
}


void BasicInstance::checkInstanceExtensionSupport() {
    vector<string> missingExtensions{};

    for(auto& extension : _requiredExt)
        if(!std::ranges::contains(_availableExt, extension)) missingExtensions.emplace_back(extension);

    CTH_STABLE_ERR(!missingExtensions.empty(), "instance extensions missing") {
        std::ranges::for_each(missingExtensions, [&details](string_view const extension) { details->add(extension); });

        throw details->exception();
    }
}
void BasicInstance::checkValidationLayerSupport() {
    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
        vector<string> missingLayers{};

        std::ranges::for_each(VALIDATION_LAYERS, [&](string_view const layer) {
            if(!std::ranges::contains(_availableLayers, layer)) missingLayers.emplace_back(layer);
        });
        CTH_STABLE_ERR(!missingLayers.empty(), "validation layers missing") {
            std::ranges::for_each(missingLayers, [&details](string_view const layer) { details->add(layer); });

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
    std::ranges::transform(availableLayers, layers.begin(), [](VkLayerProperties const& layer) { return string(layer.layerName); });

    return layers;
}

vector<string> BasicInstance::getAvailableInstanceExtensions() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    vector<string> availableExtensionsStr{availableExtensions.size()};
    std::ranges::transform(availableExtensions, availableExtensionsStr.begin(),
        [](VkExtensionProperties const& ext) { return ext.extensionName; });


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
    CTH_WARN(vk_instance == nullptr, "vk_instance invalid") {}

    vkDestroyInstance(vk_instance, nullptr);

    cth::log::msg<except::LOG>("destroyed instance");
}

#ifdef CONSTANT_DEBUG_MODE
void BasicInstance::debug_check(BasicInstance const* instance) {
    CTH_ERR(instance == nullptr, "instance must not be nullptr") throw details->exception();
    debug_check_handle(instance->get());
}
void BasicInstance::debug_check_handle(VkInstance vk_instance) {
    CTH_ERR(vk_instance == VK_NULL_HANDLE, "vk_instance invalid (VK_NULL_HANDLE)") throw details->exception();
}
void BasicInstance::debug_check_leak(BasicInstance const* instance) {
    CTH_WARN(instance->_handle != VK_NULL_HANDLE, "instance replaced (potential memory leak)") {}
}
#endif


} // namespace cth


namespace cth::vk {
using namespace std;

Instance::Instance(string_view const app_name, span<string const> const required_extensions) : BasicInstance(app_name, required_extensions) {
    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
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
void Instance::create(std::optional<BasicDebugMessenger::Config> const& messenger_config) {
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
    _debugMessenger = make_unique<DebugMessenger>(this, config);
#endif
}



} // namespace cth
