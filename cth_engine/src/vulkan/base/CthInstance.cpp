#include "CthInstance.hpp"

#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"

#ifdef CONSTANT_DEBUG_MODE
#include "../debug/CthDebugMessenger.hpp"
#endif


namespace cth::vk {

using std::string;
using std::string_view;
using std::vector;
using std::span;


Instance::Instance(string_view app_name, span<string const> required_extensions) : _name(app_name),
    _availableExt(getAvailableInstanceExtensions()) {
    _requiredExt.reserve(required_extensions.size() + REQUIRED_INSTANCE_EXTENSIONS.size());
    _requiredExt.insert(_requiredExt.end(), required_extensions.begin(), required_extensions.end());
    _requiredExt.insert(_requiredExt.end(), REQUIRED_INSTANCE_EXTENSIONS.begin(), REQUIRED_INSTANCE_EXTENSIONS.end());

    checkInstanceExtensionSupport();


    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
        _availableLayers = getAvailableValidationLayers();
        checkValidationLayerSupport();

        _requiredExt.insert(_requiredExt.begin(), VALIDATION_LAYER_EXTENSIONS.begin(), VALIDATION_LAYER_EXTENSIONS.end());
    }
}
Instance::Instance(std::string_view app_name, std::span<std::string const> required_extensions,
    std::optional<DebugMessenger::Config> const& messenger_config) :
    Instance{app_name, required_extensions} { create(messenger_config); }

Instance::Instance(std::string_view app_name, std::span<std::string const> required_extensions, State state) : Instance{app_name,
    required_extensions} { wrap(std::move(state)); }

void Instance::wrap(State state) {
    optDestroy();

    _handle = state.vkInstance.get();
    _debugMessenger = std::move(state.debugMessenger);
}

void Instance::create(std::optional<DebugMessenger::Config> messenger_config) {
    optDestroy();

#ifdef CONSTANT_DEBUG_MODE
    if(messenger_config == std::nullopt)
        messenger_config = DebugMessenger::Config::Default();
#endif

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

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if constexpr(constants::ENABLE_VALIDATION_LAYERS)
        if(messenger_config != std::nullopt) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

            debugCreateInfo = messenger_config->createInfo();
            createInfo.pNext = &debugCreateInfo;
        }

    VkInstance ptr = VK_NULL_HANDLE;
    VkResult const createInstanceResult = vkCreateInstance(&createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createInstanceResult != VK_SUCCESS, "failed to create instance!") {
        reset();
        throw cth::vk::result_exception{createInstanceResult, details->exception()};
    }


    _handle = ptr;


    if(messenger_config != std::nullopt) _debugMessenger = std::make_unique<DebugMessenger>(*messenger_config, this);
}
void Instance::destroy() {
    if(_debugMessenger) _debugMessenger = nullptr;
    destroy(_handle.get());

    reset();
}


void Instance::checkInstanceExtensionSupport() {
    vector<string> missingExtensions{};

    for(auto& extension : _requiredExt)
        if(!std::ranges::contains(_availableExt, extension)) missingExtensions.emplace_back(extension);

    CTH_STABLE_ERR(!missingExtensions.empty(), "instance extensions missing") {
        std::ranges::for_each(missingExtensions, [&details](string_view extension) { details->add(extension); });

        throw details->exception();
    }
}
void Instance::checkValidationLayerSupport() {
    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
        vector<string> missingLayers{};

        std::ranges::for_each(VALIDATION_LAYERS, [&](string_view layer) {
            if(!std::ranges::contains(_availableLayers, layer)) missingLayers.emplace_back(layer);
        });
        CTH_STABLE_ERR(!missingLayers.empty(), "validation layers missing") {
            std::ranges::for_each(missingLayers, [&details](string_view layer) { details->add(layer); });

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
    std::ranges::transform(availableLayers, layers.begin(), [](VkLayerProperties const& layer) { return string(layer.layerName); });

    return layers;
}

vector<string> Instance::getAvailableInstanceExtensions() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    vector<string> availableExtensionsStr{availableExtensions.size()};
    std::ranges::transform(availableExtensions, availableExtensionsStr.begin(),
        [](VkExtensionProperties const& ext) { return ext.extensionName; });


    return availableExtensionsStr;
}
VkApplicationInfo Instance::appInfo() const {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = _name.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = _name.c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(1, 2, 0);
    appInfo.apiVersion = VK_MAKE_VERSION(1, 2, 0);
    return appInfo;
}
void Instance::destroy(VkInstance vk_instance) {
    CTH_WARN(vk_instance == nullptr, "vk_instance invalid") {}

    vkDestroyInstance(vk_instance, nullptr);

    cth::log::msg<except::LOG>("destroyed instance");
}
void Instance::reset() {
    _debugMessenger = nullptr;
    _handle = VK_NULL_HANDLE;
}

#ifdef CONSTANT_DEBUG_MODE
void Instance::debug_check(not_null<Instance const*> instance) { debug_check_handle(instance->get()); }
void Instance::debug_check_handle([[maybe_unused]] vk::not_null<VkInstance> vk_instance) {}
#endif


} // namespace cth
