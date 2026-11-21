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
    [[nodiscard]] vector<string> missing(
        std::ranges::range auto const& names,
        vector<string> const& available
    ) {
        vector<string> missing{};
        for(string_view const name : names)
            if(!std::ranges::contains(available, name))
                missing.emplace_back(name);
        return missing;
    }
}

Instance::Instance(Config config) : _config{std::move(config)} {}

Instance::Instance(Config config, State state) : Instance{std::move(config)} {
    wrap(std::move(state));
}

Instance::Instance(Config config, create_t) : Instance{std::move(config)} { create(); }

void Instance::wrap(State state) {
    auto const messenger = messengerConfig();
    CTH_CRITICAL(
        state.debugMessenger.has_value() != messenger.has_value(),
        "the state must have a messenger <=> the config has one ({}), got {}",
        messenger.has_value(),
        state.debugMessenger.has_value()
    ) {}

    optDestroy();

    _handle.reset(state.vkInstance.get());
    _extensions = std::move(state.extensions);

    loadInstanceFunctions(state.vkInstance);

    if(state.debugMessenger)
        _debugMessengerOpt.emplace(*this, *messenger, *state.debugMessenger);
}

void Instance::create() {
    optDestroy();

    auto const messenger = messengerConfig();

    // the members change after the checks, a throw from them finds the members forgotten
    auto requiredExtensions = _config.extensions;
    if(messenger.has_value())
        for(string_view const extension : MESSENGER_EXTENSIONS)
            if(!std::ranges::contains(requiredExtensions, extension))
                requiredExtensions.emplace_back(extension);

    auto const missingExtensions = missing(requiredExtensions, available_instance_extensions());
    CTH_STABLE_ERR(!missingExtensions.empty(), "instance extensions missing") {
        for(auto const& extension : missingExtensions)
            details->add(extension);
        throw cvk::cvk_exception{details->exception()};
    }

    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
        auto const missingLayers = missing(VALIDATION_LAYERS, available_layers());
        CTH_STABLE_ERR(!missingLayers.empty(), "validation layers missing") {
            for(auto const& layer : missingLayers)
                details->add(layer);
            throw cvk::cvk_exception{details->exception()};
        }
    }

    _extensions = std::move(requiredExtensions);
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
        createInfo.pNext = &layerSettings;
    }

    if(messenger.has_value()) {
        // the messenger of the create info reports the messages of vkCreateInstance() and vkDestroyInstance()
        debugCreateInfo = messenger->createInfo();
        debugCreateInfo.pNext = createInfo.pNext;
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

    // the messenger needs the created instance, only a catch can destroy the instance when the messenger fails
    try {
        if(messenger)
            _debugMessengerOpt.emplace(*this, *messenger, cvk::create);
    }
    catch(...) {
        destroy();
        throw;
    }
}

void Instance::destroy() {
    debug_check(*this);

    _debugMessengerOpt.reset();
    _handle.reset();

    forget();
}

Instance::State Instance::release() {
    debug_check(*this);

    State state{
        .vkInstance = _handle.release(),
        .debugMessenger = _debugMessengerOpt.transform(
            [](DebugMessenger& debugMessenger) { return debugMessenger.release(); }
        ),
        .extensions = std::move(_extensions),
    };
    forget();
    return state;
}

void destroy(VkInstance vkInstance) {
    CTH_WARN(vkInstance == nullptr, "vkInstance invalid") {}

    vkDestroyInstance(vkInstance, nullptr);

    log::msg<except::LOG>("destroyed instance");
}

void Instance::forget() {
    _debugMessengerOpt.reset();
    _extensions.clear();
}

void Instance::loadInstanceFunctions(vk_not_null<VkInstance> vkInstance) {
    volkLoadInstanceOnly(vkInstance.get());
}
}

namespace cvk {
vector<string> available_instance_extensions() {
    auto const properties = dev::enumerate(
        "instance extensions",
        vkEnumerateInstanceExtensionProperties,
        nullptr
    );

    return {
        std::from_range,
        properties | std::views::transform(
            [](VkExtensionProperties const& extension) { return string{extension.extensionName}; }
        )
    };
}

vector<string> available_layers() {
    auto const properties = dev::enumerate("instance layers", vkEnumerateInstanceLayerProperties);

    return {
        std::from_range,
        properties | std::views::transform(
            [](VkLayerProperties const& layer) { return string{layer.layerName}; }
        )
    };
}
}
