#include "CthInstance.hpp"
#include "../utils/CthDebugMessenger.hpp"

#include <cth/cth_exception.hpp>
#include <cth/cth_log.hpp>

namespace cth {

Instance::Instance(const string& app_name, const vector<const char*>& required_extensions) : name(name), requiredExt{required_extensions},
    availableExt{getAvailableInstanceExtensions()} {

    if constexpr(ENABLE_VALIDATION_LAYERS) {
        availableLayers = getAvailableValidationLayers();
        checkValidationLayerSupport();
        debugMessenger = make_unique<DebugMessenger>();

        requiredExt.insert(requiredExt.begin(), VALIDATION_LAYER_EXTENSIONS.begin(), VALIDATION_LAYER_EXTENSIONS.end());
    }
    checkInstanceExtensionSupport();

    const auto createInfo = this->createInfo();
    const VkResult createInstanceResult = vkCreateInstance(&createInfo, nullptr, &vkInstance);
    CTH_STABLE_ERR(createInstanceResult == VK_SUCCESS, "VK: failed to create instance!")
        throw cth::except::data_exception{createInstanceResult, details->exception()};


    debugMessenger->init(vkInstance); //IMPORTANT
}
Instance::~Instance() { vkDestroyInstance(vkInstance, nullptr); }

VkApplicationInfo Instance::appInfo() const {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = name.c_str(); //TEMP declare a variable for this
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_MAKE_API_VERSION(1, 3, 275, 0);
    return appInfo;
}
VkInstanceCreateInfo Instance::createInfo() const {
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    const auto appInfo = this->appInfo();
    createInfo.pApplicationInfo = &appInfo;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExt.size());
    createInfo.ppEnabledExtensionNames = requiredExt.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if constexpr(ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

        debugCreateInfo = debugMessenger->createInfo();
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    return createInfo;
}

void Instance::checkInstanceExtensionSupport() {
    vector<const char*> missingExtensions{};

    ranges::for_each(requiredExt, [&](const char* extension) {
        if(!ranges::contains(availableExt, extension)) missingExtensions.push_back(extension);
    });
    CTH_STABLE_ERR(missingExtensions.empty(), "missing instance extensions:") {
        ranges::for_each(missingExtensions, [&details](const char* extension) { details->add(extension); });

        throw details->exception();
    }
}
void Instance::checkValidationLayerSupport() {
    if constexpr(ENABLE_VALIDATION_LAYERS) {
        vector<const char*> missingLayers{};

        ranges::for_each(VALIDATION_LAYERS, [&](const char* layer) { if(!ranges::contains(availableLayers, layer)) missingLayers.push_back(layer); });
        CTH_STABLE_ERR(missingLayers.empty(), "missing validation layers:") {
            ranges::for_each(missingLayers, [&details](const char* layer) { details->add(layer); });

            throw details->exception();
        }
    }
}
vector<const char*> Instance::getAvailableValidationLayers() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    vector<const char*> layers(availableLayers.size());
    ranges::transform(availableLayers, layers.begin(), [](const VkLayerProperties& layer) { return layer.layerName; });

    return layers;
}
vector<const char*> Instance::getAvailableInstanceExtensions() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    vector<const char*> availableExtensionsStr{availableExtensions.size()};
    ranges::transform(availableExtensions, availableExtensionsStr.begin(),
        [](const VkExtensionProperties& ext) { return ext.extensionName; });


    return availableExtensionsStr;
}

} // namespace cth
