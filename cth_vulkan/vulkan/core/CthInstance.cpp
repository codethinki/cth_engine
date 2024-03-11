#include "CthInstance.hpp"

#include "../utils/CthDebugMessenger.hpp"
#include "../utils/cth_vk_specific_utils.hpp"

#include <cth/cth_log.hpp>



namespace cth {

Instance::Instance(string app_name, const vector<string>& required_extensions) : name(std::move(app_name)), requiredExt{required_extensions},
    availableExt{getAvailableInstanceExtensions()} {

    if constexpr(ENABLE_VALIDATION_LAYERS) {
        availableLayers = getAvailableValidationLayers();
        checkValidationLayerSupport();
        debugMessenger = make_unique<DebugMessenger>();

        requiredExt.insert(requiredExt.begin(), VALIDATION_LAYER_EXTENSIONS.begin(), VALIDATION_LAYER_EXTENSIONS.end());
    }
    checkInstanceExtensionSupport();

    create();

    debugMessenger->init(vkInstance); //IMPORTANT
}
Instance::~Instance() { vkDestroyInstance(vkInstance, nullptr); }

VkApplicationInfo Instance::appInfo() const {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = name.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = name.c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(1, 2, 0);
    appInfo.apiVersion = VK_MAKE_VERSION(1, 2, 0);
    return appInfo;
}
void Instance::create() {
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    const auto appInfo = this->appInfo();
    createInfo.pApplicationInfo = &appInfo;

    const auto requiredExtVec = toCharVec(requiredExt);
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtVec.size());
    createInfo.ppEnabledExtensionNames = requiredExtVec.data();

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

    const VkResult createInstanceResult = vkCreateInstance(&createInfo, nullptr, &vkInstance);
    CTH_STABLE_ERR(createInstanceResult != VK_SUCCESS, "VK: failed to create instance!")
        throw cth::except::vk_result_exception{createInstanceResult, details->exception()};
}

void Instance::checkInstanceExtensionSupport() {
    vector<string> missingExtensions{};

    ranges::for_each(requiredExt, [&](const string_view extension) {
        if(!ranges::contains(availableExt, extension)) missingExtensions.emplace_back(extension);
    });
    CTH_STABLE_ERR(!missingExtensions.empty(), "instance extensions missing") {
        ranges::for_each(missingExtensions, [&details](const string_view extension) { details->add(extension); });

        throw details->exception();
    }
}
void Instance::checkValidationLayerSupport() {
    if constexpr(ENABLE_VALIDATION_LAYERS) {
        vector<string> missingLayers{};

        ranges::for_each(VALIDATION_LAYERS, [&](const string_view layer) { if(!ranges::contains(availableLayers, layer)) missingLayers.emplace_back(layer); });
        CTH_STABLE_ERR(!missingLayers.empty(), "validation layers missing") {
            ranges::for_each(missingLayers, [&details](const string_view layer) { details->add(layer); });

            throw details->exception();
        }
    }
}
vector<string> Instance::getAvailableValidationLayers() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    vector<string> layers(availableLayers.size());
    ranges::transform(availableLayers, layers.begin(), [](const VkLayerProperties& layer) { return string(layer.layerName); });

    return layers;
}
vector<string> Instance::getAvailableInstanceExtensions() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    vector<string> availableExtensionsStr{availableExtensions.size()};
    ranges::transform(availableExtensions, availableExtensionsStr.begin(),
        [](const VkExtensionProperties& ext) { return ext.extensionName; });


    return availableExtensionsStr;
}

} // namespace cth
