#include "CthInstance.hpp"

#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


#include "../debug/CthDebugMessenger.hpp"



namespace cth::vk {

using std::string;
using std::string_view;
using std::vector;
using std::span;


Instance::Instance(string_view app_name, span<string const> required_extensions) : _name(app_name), _availableExt{getAvailableInstanceExtensions()} {
    _requiredExt.reserve(required_extensions.size() + REQUIRED_INSTANCE_EXTENSIONS.size());
    _requiredExt.append_range(required_extensions);
    _requiredExt.append_range(REQUIRED_INSTANCE_EXTENSIONS | std::views::transform([](std::string_view const& view) { return std::string{view}; }));

    checkInstanceExtensionSupport();


    if constexpr(constants::ENABLE_VALIDATION_LAYERS) enableValidationLayers();
}

Instance::Instance(std::string_view app_name, std::span<std::string const> required_extensions,
    std::optional<DebugMessenger::Config> const& messenger_config) : Instance{app_name, required_extensions} { create(messenger_config); }

Instance::Instance(std::string_view app_name, std::span<std::string const> required_extensions, State state) : Instance{app_name,
    required_extensions} { wrap(std::move(state)); }

void Instance::wrap(State state) {
    optDestroy();

    _handle = state.vkInstance.get();
    _debugMessenger = std::move(state.debugMessenger);
}

void Instance::create(std::optional<DebugMessenger::Config> messenger_config) {
    optDestroy();

    if constexpr(COMPILATION_MODE == CompilationMode::DEBUG) {
        if(messenger_config == std::nullopt)
            messenger_config = DebugMessenger::Config::Default();
    }

    vector<char const*> requiredExtVec(_requiredExt.size());
    std::ranges::transform(_requiredExt, requiredExtVec.begin(), [](auto const& str) { return str.data(); });

    auto const appInfo = this->appInfo();
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,

        .enabledExtensionCount = static_cast<uint32_t>(requiredExtVec.size()),
        .ppEnabledExtensionNames = requiredExtVec.data(),
    };

    if constexpr(constants::ENABLE_VALIDATION_LAYERS)
        if(messenger_config != std::nullopt) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

            debugCreateInfo = messenger_config->createInfo();
            createInfo.pNext = &debugCreateInfo;
        }

    VkInstance ptr = VK_NULL_HANDLE;
    auto const createInstanceResult = vkCreateInstance(&createInfo, nullptr, &ptr);

    CTH_STABLE_ERR(createInstanceResult != VK_SUCCESS, "failed to create instance!") {
        reset();
        throw cth::vk::result_exception{createInstanceResult, details->exception()};
    }

    _handle = ptr;
    loadInstanceFunctions(ptr);

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
void Instance::enableValidationLayers() {
    _availableLayers = getAvailableValidationLayers();
    checkValidationLayerSupport();

    _requiredExt.insert(_requiredExt.begin(), VALIDATION_LAYER_EXTENSIONS.begin(), VALIDATION_LAYER_EXTENSIONS.end());
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
    return VkApplicationInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = _name.c_str(),
        .applicationVersion = 0,
        .pEngineName = _name.c_str(),
        .engineVersion = 0,
        .apiVersion = VK_API_VERSION_1_0,
    };
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

void Instance::loadInstanceFunctions(cth::vk::not_null<VkInstance> vk_instance) {
    if(volkGetLoadedInstance() == VK_NULL_HANDLE)
        volkLoadInstanceOnly(vk_instance.get());
}

}
