#include "cvk/base/instance.hpp"

#include "cvk/debug/debug_messenger.hpp"
#include "cvk/utility/enumerate.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/string.hpp>

#include <algorithm>
#include <ranges>
#include <string_view>


namespace cvk {
using std::string;
using std::string_view;
using std::vector;

namespace {
    /**
     * @return the names that are not available
     */
    [[nodiscard]] vector<string> missing(std::ranges::range auto const& names, vector<string> const& available) {
        vector<string> missing{};
        for(string_view const name : names)
            if(!std::ranges::contains(available, name)) missing.emplace_back(name);
        return missing;
    }
}

Instance::Instance(Config config) : _config{std::move(config)} {}

Instance::Instance(Config config, State state) : Instance{std::move(config)} { wrap(std::move(state)); }

Instance::Instance(Config config, create_t, std::optional<DebugMessenger::Config> messengerConfig) :
    Instance{std::move(config)} {
    create(std::move(messengerConfig));
}

void Instance::wrap(State state) {
    optDestroy();

    _handle.reset(state.vkInstance.get());
    _debugMessenger = std::move(state.debugMessenger);
    _extensions = std::move(state.extensions);

    loadInstanceFunctions(state.vkInstance);
}

void Instance::create(std::optional<DebugMessenger::Config> messengerConfig) {
    optDestroy();

    _extensions = _config.extensions;
    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
        for(string_view const extension : VALIDATION_LAYER_EXTENSIONS)
            if(!std::ranges::contains(_extensions, extension)) _extensions.emplace_back(extension);

        if(!messengerConfig.has_value()) messengerConfig = DebugMessenger::Config::Default();
    }

    auto const missingExtensions = missing(_extensions, available_instance_extensions());
    CTH_STABLE_ERR(!missingExtensions.empty(), "instance extensions missing") {
        for(auto const& extension : missingExtensions)
            details->add(extension);
        forget();
        throw cvk::cvk_exception{details->exception()};
    }

    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
        auto const missingLayers = missing(VALIDATION_LAYERS, available_layers());
        CTH_STABLE_ERR(!missingLayers.empty(), "validation layers missing") {
            for(auto const& layer : missingLayers)
                details->add(layer);
            forget();
            throw cvk::cvk_exception{details->exception()};
        }
    }

    auto const extensions = cth::str::to_c_str_vector(_extensions);
    auto const appInfo = _config.appInfo();

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    VkLayerSettingsCreateInfoEXT const layerSettings{
        .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
        .pNext = nullptr,
        .settingCount = static_cast<uint32_t>(VALIDATION_LAYER_SETTINGS.size()),
        .pSettings = VALIDATION_LAYER_SETTINGS.data(),
    };
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

        // the messenger of the create info reports the messages of vkCreateInstance() and vkDestroyInstance()
        debugCreateInfo = messengerConfig->createInfo();
        debugCreateInfo.pNext = &layerSettings;
        createInfo.pNext = &debugCreateInfo;
    }

    VkInstance ptr = VK_NULL_HANDLE;
    auto const result = vkCreateInstance(&createInfo, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create instance") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr);
    loadInstanceFunctions(ptr);

    if constexpr(constants::ENABLE_VALIDATION_LAYERS)
        _debugMessenger.emplace(*this, *messengerConfig, cvk::create);
}

void Instance::destroy() {
    debug_check(*this);

    _debugMessenger.reset();
    _handle.reset();

    forget();
}

Instance::State Instance::release() {
    debug_check(*this);

    State state{
        .vkInstance = _handle.release(),
        .debugMessenger = std::move(_debugMessenger),
        .extensions = std::move(_extensions),
    };
    forget();
    return state;
}

void Instance::destroy(VkInstance vkInstance) {
    CTH_WARN(vkInstance == nullptr, "vkInstance invalid") {}

    vkDestroyInstance(vkInstance, nullptr);

    log::msg<except::LOG>("destroyed instance");
}

void Instance::forget() {
    _debugMessenger.reset();
    _extensions.clear();
}

void Instance::loadInstanceFunctions(vk_not_null<VkInstance> vkInstance) { volkLoadInstanceOnly(vkInstance.get()); }
}

namespace cvk {
vector<string> available_instance_extensions() {
    auto const properties = dev::enumerate("instance extensions", vkEnumerateInstanceExtensionProperties, nullptr);

    return {
        std::from_range,
        properties | std::views::transform([](VkExtensionProperties const& extension) {
            return string{extension.extensionName};
        })
    };
}

vector<string> available_layers() {
    auto const properties = dev::enumerate("instance layers", vkEnumerateInstanceLayerProperties);

    return {
        std::from_range,
        properties | std::views::transform([](VkLayerProperties const& layer) { return string{layer.layerName}; })
    };
}
}
