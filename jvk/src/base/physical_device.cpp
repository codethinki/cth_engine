#include "jvk/base/physical_device.hpp"

#include "jvk/base/instance.hpp"
#include "jvk/base/queue/queue.hpp"
#include "jvk/surface/surface.hpp"
#include "jvk/utility/vk_exceptions.hpp"

#include <unordered_set>


#include <range/v3/view/concat.hpp>


namespace jvk {
using std::vector;
using std::string_view;
using std::span;
using std::unique_ptr;



PhysicalDevice::PhysicalDevice(
    Instance const& instance,
    utils::PhysicalDeviceFeatures required_features,
    std::span<std::string const> required_extensions
) : _instance{&instance},
    _requiredFeatures{std::move(required_features)},
    _requiredExtensions{std::from_range, required_extensions} {}

PhysicalDevice::PhysicalDevice(
    Instance const& instance,
    utils::PhysicalDeviceFeatures const& required_features,
    std::span<std::string const> required_extensions,
    std::span<Surface const> surfaces,
    jvk::vk_not_null<VkPhysicalDevice> vk_device
) : PhysicalDevice{instance, required_features, required_extensions} { create(surfaces, vk_device); }

PhysicalDevice::PhysicalDevice(
    Instance const& instance,
    utils::PhysicalDeviceFeatures const& required_features,
    std::span<std::string const> required_extensions,
    State const& state
) : PhysicalDevice{instance, required_features, required_extensions} { wrap(state); }


auto PhysicalDevice::Create(
    Instance const& instance,
    std::span<Surface const> surface,
    std::span<QueueFamilyProperties const> queue_properties,
    std::span<std::string const> required_extensions,
    utils::PhysicalDeviceFeatures const& required_features,
    jvk::vk_not_null<VkPhysicalDevice> vk_device
) -> std::optional<CreateResult> {
    PhysicalDevice physicalDevice{instance, required_features, required_extensions, surface, vk_device};


    if(!physicalDevice.suitable(queue_properties))
        return std::nullopt;

    auto indices = physicalDevice.queueFamilyIndices(queue_properties);
    return CreateResult{
        std::move(physicalDevice),
        std::move(indices)
    };
}

void PhysicalDevice::wrap(State const& state) {
    auto const device = state.vkDevice;
    PhysicalDevice::debug_check_handle(device);

    _handle = device.get();

    if(!state.features.empty()) _features = state.features;
    else _features = utils::PhysicalDeviceFeatures{device, _requiredFeatures};

    if(!state.extensions.empty()) _extensions = state.extensions;
    else _extensions = queryExtensions(device);

    _properties = state.properties.value_or(queryProperties(device));

    if(_maxSampleCount != VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM) _maxSampleCount = state.maxSampleCount;
    else _maxSampleCount = evalMaxSampleCount(_properties);


    _memProperties = state.memProperties.value_or(queryMemoryProperties(device));
    _queueFamilies = state.queueFamilies;
}

void PhysicalDevice::create(std::span<Surface const> surfaces, vk_not_null<VkPhysicalDevice> vk_device) {
    CTH_CRITICAL(surfaces.empty(), "there must be at least one surface") {}
    create(queryQueueFamilies(surfaces, vk_device), vk_device);
}

void PhysicalDevice::create(std::span<QueueFamily const> queue_families, vk_not_null<VkPhysicalDevice> vk_device) {
    PhysicalDevice::debug_check_handle(vk_device);

    _handle = vk_device.get();
    _features = utils::PhysicalDeviceFeatures{vk_device, _requiredFeatures};
    _extensions = queryExtensions(vk_device);
    _properties = queryProperties(vk_device);
    _maxSampleCount = evalMaxSampleCount(_properties);
    _memProperties = queryMemoryProperties(vk_device);
    _queueFamilies = {std::from_range, queue_families};
}


bool PhysicalDevice::suitable(std::span<QueueFamilyProperties const> queues) {
    PhysicalDevice::debug_check(*this);

    auto const missingFeatures = supports(_requiredFeatures);
    auto const missingExtensions = supports(_requiredExtensions);
    auto const queueIndices = queueFamilyIndices(queues);
    bool const suitable = missingFeatures.empty() && missingExtensions.empty() && !queueIndices.empty();

    CTH_WARN(!suitable, "physical device ({}) is missing features", _properties.deviceName) {
        if(queueIndices.empty())
            details->add("missing queue families");

        for(auto const& missingExtension : missingExtensions)
            details->add(
                "missing extension: {}",
                missingExtension
            );

        for(auto const& missingFeature : missingFeatures)
            std::visit(
                cth::var::overload{
                    [&details](size_t index) { details->add("missing feature ({})", index); },
                    [&details](VkStructureType s_type) {
                        details->add("missing feature2 extension: ({})", static_cast<uint32_t>(s_type));
                    }
                },
                missingFeature
            );
    }


    return suitable;
}


auto PhysicalDevice::AutoPick(
    Instance const& instance,
    std::span<Surface const> temp_surfaces,
    std::span<QueueFamilyProperties const> queue_properties,
    span<std::string const> required_extensions,
    utils::PhysicalDeviceFeatures const& required_features
) -> std::optional<CreateResult> {
    Instance::debug_check(instance);

    auto const devices = enumerateVkDevices(instance.get());

    auto joinView = ::ranges::views::concat(required_extensions, constants::REQUIRED_DEVICE_EXTENSIONS);

    vector<std::string> requiredExtensions{std::ranges::begin(joinView), std::ranges::end(joinView)};


    utils::PhysicalDeviceFeatures requiredFeatures{constants::REQUIRED_DEVICE_FEATURES2};
    requiredFeatures.merge(required_features);


    std::optional<CreateResult> chosen{};

    for(auto& device : devices) {
        auto createResult = Create(
            instance,
            temp_surfaces,
            queue_properties,
            requiredExtensions,
            requiredFeatures,
            device
        );

        if(createResult.has_value())
            chosen = std::move(createResult);
    }

    if(!chosen)
        return std::nullopt;

    cth::log::msg<except::INFO>("chosen physical device: {}", chosen->device._properties.deviceName);

    return chosen;
}



auto PhysicalDevice::supports(
    utils::PhysicalDeviceFeatures const& required_features
) const -> std::vector<std::variant<size_t, VkStructureType>> {
    PhysicalDevice::debug_check(*this);

    return _features.supports(required_features);
}

auto PhysicalDevice::supports(std::span<std::string const> required_extensions) -> vector<std::string> {
    vector<std::string> missingExtensions{};
    for(auto const& requiredExtension : required_extensions) {
        auto const it = std::ranges::find(_extensions, requiredExtension);

        if(it == _extensions.end()) missingExtensions.emplace_back(requiredExtension);
    }

    return missingExtensions;
}

uint32_t PhysicalDevice::findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags mem_properties) const {
    PhysicalDevice::debug_check(*this);

    for(uint32_t i = 0; i < _memProperties.memoryTypeCount; i++)
        if((type_filter & (1 << i)) && (_memProperties.memoryTypes[i].propertyFlags & mem_properties) ==
            mem_properties)
            return i;

    CTH_STABLE_ERR(true, "no suitable memory type available")
    throw details->exception();
}

auto PhysicalDevice::findSupportedFormat(
    span<VkFormat const> candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
) const -> VkFormat {
    PhysicalDevice::debug_check(*this);

    for(VkFormat const format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(_handle.get(), format, &props);

        if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return
                format;
        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return
                format;
    }
    CTH_STABLE_ERR(true, "format unsupported")
    throw except::data_exception{features, details->exception()};
}

auto PhysicalDevice::queueFamilyIndices(
    span<QueueFamilyProperties const> queues_properties
) const -> vector<queue_family_index_t> {
    debug_check(*this);

    if(queues_properties.empty())
        return {};

    vector<vector<queue_family_index_t>> queueIndices{queues_properties.size()};

    for(auto [requiredProperties, indices] : std::views::zip(queues_properties, queueIndices)) {
        for(auto const& family : _queueFamilies) {
            bool const support = (family.properties & requiredProperties) == requiredProperties;

            if(support)
                indices.push_back(family.index);
        }
    }
    std::vector<size_t> familiesMaxQueues(_queueFamilies.size());

    for(auto [src, dst] : std::views::zip(_queueFamilies, familiesMaxQueues))
        dst = src.vkProperties.queueCount;

    auto const result = cth::alg::assign(queueIndices, familiesMaxQueues);
    return result;
}

bool PhysicalDevice::supportsQueueSet(span<QueueFamilyProperties const> queue_properties) const {
    PhysicalDevice::debug_check(*this);

    return !queueFamilyIndices(queue_properties).empty();
}

vector<VkPhysicalDevice> PhysicalDevice::enumerateVkDevices(jvk::vk_not_null<VkInstance> vk_instance) {
    Instance::debug_check_handle(vk_instance);

    uint32_t deviceCount = 0;
    auto const countResult = vkEnumeratePhysicalDevices(vk_instance.get(), &deviceCount, nullptr);
    CTH_STABLE_ERR(countResult != VK_SUCCESS, "failed to count physical devices")
    throw vk_result_exception{countResult, details->exception()};

    std::vector<VkPhysicalDevice> devices(deviceCount);
    auto const writeResult = vkEnumeratePhysicalDevices(vk_instance.get(), &deviceCount, devices.data());
    CTH_STABLE_ERR(writeResult != VK_SUCCESS, "failed to write physical devices")
    throw vk_result_exception{writeResult, details->exception()};

    return devices;
}

vector<std::string> PhysicalDevice::queryExtensions(jvk::vk_not_null<VkPhysicalDevice> vk_device) {
    uint32_t extensionCount = 0;
    auto const countResult = vkEnumerateDeviceExtensionProperties(
        vk_device.get(),
        nullptr,
        &extensionCount,
        nullptr
    );
    CTH_STABLE_ERR(countResult != VK_SUCCESS, "failed to count device extensions")
    throw vk_result_exception{countResult, details->exception()};

    vector<VkExtensionProperties> availableExtensions{extensionCount};
    auto const writeResult = vkEnumerateDeviceExtensionProperties(
        vk_device.get(),
        nullptr,
        &extensionCount,
        availableExtensions.data()
    );
    CTH_STABLE_ERR(writeResult != VK_SUCCESS, "failed to write device extension properties")
    throw vk_result_exception{writeResult, details->exception()};

    vector<std::string> extensions{extensionCount};

    std::ranges::transform(
        availableExtensions,
        extensions.begin(),
        [](VkExtensionProperties const& extension) { return extension.extensionName; }
    );

    return extensions;
}

VkPhysicalDeviceProperties PhysicalDevice::queryProperties(jvk::vk_not_null<VkPhysicalDevice> vk_device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(vk_device.get(), &properties);
    return properties;
}

VkPhysicalDeviceMemoryProperties PhysicalDevice::queryMemoryProperties(
    jvk::vk_not_null<VkPhysicalDevice> vk_device
) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vk_device.get(), &memProperties);
    return memProperties;
}

std::vector<QueueFamily> PhysicalDevice::queryQueueFamilies(
    Surface const& surface,
    jvk::vk_not_null<VkPhysicalDevice> vk_device
) {
    vector<VkQueueFamilyProperties> familyProperties{};
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk_device.get(), &count, nullptr);
    familyProperties.resize(count);
    vkGetPhysicalDeviceQueueFamilyProperties(vk_device.get(), &count, familyProperties.data());


    vector<bool> presentSupport(familyProperties.size());

    for(uint32_t i = 0; i < familyProperties.size(); i++) {
        VkBool32 support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(vk_device.get(), i, surface.get(), &support);
        presentSupport[i] = support;
    }

    std::vector<QueueFamily> queueFamilies{};

    queueFamilies.reserve(presentSupport.size());
    for(size_t i = 0; i < presentSupport.size(); i++)
        queueFamilies.push_back(
            QueueFamily::Vk(static_cast<uint32_t>(i), familyProperties[i], presentSupport[i])
        );

    return queueFamilies;
}

std::vector<QueueFamily> PhysicalDevice::queryQueueFamilies(
    std::span<Surface const> surfaces,
    vk_not_null<VkPhysicalDevice> vk_device
) {
    CTH_CRITICAL(surfaces.empty(), "no surfaces provided"){}

    std::unordered_set<QueueFamily> queueFamilies{};
    queueFamilies.insert_range(queryQueueFamilies(surfaces.front(), vk_device));

    for(auto const& surface : surfaces) {
        std::unordered_set families = {std::from_range, queryQueueFamilies(surface, vk_device)};
        std::erase_if(
            queueFamilies,
            [&families](auto const& queue_family) { return !families.contains(queue_family); }
        );
    }

    return {std::from_range, queueFamilies};
}

VkSampleCountFlagBits PhysicalDevice::evalMaxSampleCount(VkPhysicalDeviceProperties const& properties) {
    VkSampleCountFlags const supportedSamples = properties.limits.framebufferColorSampleCounts &
        properties.limits.framebufferDepthSampleCounts;


    cxpr std::array sampleFlagBits = {
        VK_SAMPLE_COUNT_64_BIT,
        VK_SAMPLE_COUNT_32_BIT,
        VK_SAMPLE_COUNT_16_BIT,
        VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_4_BIT,
        VK_SAMPLE_COUNT_2_BIT,
        VK_SAMPLE_COUNT_1_BIT
    };


    for(auto const sampleFlagBit : sampleFlagBits)
        if(supportedSamples & sampleFlagBit)
            return sampleFlagBit;

    CTH_CRITICAL(true, "invalid state, sample count not supported") {}
    return VK_SAMPLE_COUNT_1_BIT;
}
QueueFamily const& PhysicalDevice::queueFamily(queue_family_index_t idx) const {
    _queueFamilies | std::ranges::find_if()
}


}
