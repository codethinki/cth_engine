#include "cvk/base/physical_device.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constant/device_constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/algorithm/combine.hpp>

#include <algorithm>
#include <ranges>


namespace cvk {
using std::span;
using std::string;
using std::vector;

PhysicalDevice::PhysicalDevice(
    Instance const& instance,
    utils::PhysicalDeviceFeatures requiredFeatures,
    span<string const> requiredExtensions
) : _instance{&instance},
    _requiredFeatures{std::move(requiredFeatures)},
    _requiredExtensions{std::from_range, requiredExtensions} {}

PhysicalDevice::PhysicalDevice(
    Instance const& instance,
    utils::PhysicalDeviceFeatures const& requiredFeatures,
    span<string const> requiredExtensions,
    span<Surface const> surfaces,
    vk_not_null<VkPhysicalDevice> vkDevice
) : PhysicalDevice{instance, requiredFeatures, requiredExtensions} { create(surfaces, vkDevice); }

PhysicalDevice::PhysicalDevice(
    Instance const& instance,
    utils::PhysicalDeviceFeatures const& requiredFeatures,
    span<string const> requiredExtensions,
    State const& state
) : PhysicalDevice{instance, requiredFeatures, requiredExtensions} { wrap(state); }


auto PhysicalDevice::Create(
    Instance const& instance,
    span<Surface const> surfaces,
    span<QueueFamilyProperties const> queueProperties,
    span<string const> requiredExtensions,
    utils::PhysicalDeviceFeatures const& requiredFeatures,
    vk_not_null<VkPhysicalDevice> vkDevice
) -> std::optional<CreateResult> {
    PhysicalDevice physicalDevice{instance, requiredFeatures, requiredExtensions, surfaces, vkDevice};

    if(!physicalDevice.suitable(queueProperties))
        return std::nullopt;

    auto indices = physicalDevice.queueFamilyIndices(queueProperties);
    return CreateResult{
        std::move(physicalDevice),
        std::move(indices)
    };
}

void PhysicalDevice::wrap(State const& state) {
    auto const device = state.vkDevice;
    debug_check_handle(device);

    _handle = device.get();

    _features = state.features.has_value() ? *state.features : utils::PhysicalDeviceFeatures{device};
    _extensions = !state.extensions.empty() ? state.extensions : queryExtensions(device);
    _properties = state.properties.has_value() ? *state.properties : queryProperties(device);

    _maxSampleCount = state.maxSampleCount != VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM
        ? state.maxSampleCount
        : evalMaxSampleCount(_properties);

    _memProperties = state.memProperties.has_value() ? *state.memProperties : queryMemoryProperties(device);
    _queueFamilies = !state.queueFamilies.empty() ? state.queueFamilies : queryQueueFamilies({}, device);
}

void PhysicalDevice::create(span<Surface const> surfaces, vk_not_null<VkPhysicalDevice> vkDevice) {
    create(queryQueueFamilies(surfaces, vkDevice), vkDevice);
}

void PhysicalDevice::create(span<QueueFamily const> queueFamilies, vk_not_null<VkPhysicalDevice> vkDevice) {
    debug_check_handle(vkDevice);

    _handle = vkDevice.get();
    _features = utils::PhysicalDeviceFeatures{vkDevice};
    _extensions = queryExtensions(vkDevice);
    _properties = queryProperties(vkDevice);
    _maxSampleCount = evalMaxSampleCount(_properties);
    _memProperties = queryMemoryProperties(vkDevice);
    _queueFamilies = {std::from_range, queueFamilies};
}


bool PhysicalDevice::suitable(span<QueueFamilyProperties const> queueProperties) const {
    debug_check(*this);

    auto const missingFeatures = supports(_requiredFeatures);
    auto const missingExtensions = supports(_requiredExtensions);
    auto const queueIndices = queueFamilyIndices(queueProperties);
    bool const suitable = missingFeatures.empty() && missingExtensions.empty() && !queueIndices.empty();

    CTH_WARN(!suitable, "physical device ({}) is not suitable", _properties.deviceName) {
        if(queueIndices.empty())
            details->add("missing queue families");

        for(auto const& missingExtension : missingExtensions)
            details->add("missing extension: {}", missingExtension);

        for(auto const& missingFeature : missingFeatures)
            details->add("missing feature: {}", missingFeature);
    }

    return suitable;
}

auto PhysicalDevice::AutoPick(
    Instance const& instance,
    span<Surface const> surfaces,
    span<QueueFamilyProperties const> queueProperties,
    span<string const> requiredExtensions,
    utils::PhysicalDeviceFeatures const& requiredFeatures
) -> std::optional<CreateResult> {
    Instance::debug_check(instance);

    auto const devices = enumerateVkDevices(instance.get());

    auto const toString = [](std::string_view extension) { return string{extension}; };

    vector<string> extensions{std::from_range, requiredExtensions};
    extensions.append_range(constants::REQUIRED_DEVICE_EXTENSIONS | std::views::transform(toString));

    bool const present = std::ranges::any_of(
        queueProperties,
        [](QueueFamilyProperties properties) { return contains(properties, QueueFamilyProperties::PRESENT); }
    );
    if(present) extensions.append_range(constants::PRESENT_DEVICE_EXTENSIONS | std::views::transform(toString));

    auto features = constants::required_device_features();
    features.merge(requiredFeatures);


    std::optional<CreateResult> chosen{};

    for(auto const& device : devices) {
        auto createResult = Create(instance, surfaces, queueProperties, extensions, features, device);

        if(createResult.has_value()) {
            chosen = std::move(createResult);
            break;
        }
    }

    if(!chosen)
        return std::nullopt;

    log::msg<except::INFO>("chosen physical device: {}", chosen->device._properties.deviceName);

    return chosen;
}

auto PhysicalDevice::supports(
    utils::PhysicalDeviceFeatures const& requiredFeatures
) const -> vector<utils::MissingFeature> {
    debug_check(*this);

    return _features.supports(requiredFeatures);
}

auto PhysicalDevice::supports(span<string const> requiredExtensions) const -> vector<string> {
    vector<string> missingExtensions{};
    for(auto const& requiredExtension : requiredExtensions)
        if(!std::ranges::contains(_extensions, requiredExtension)) missingExtensions.emplace_back(requiredExtension);

    return missingExtensions;
}

auto PhysicalDevice::findSupportedFormat(
    span<VkFormat const> candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
) const -> VkFormat {
    debug_check(*this);

    for(VkFormat const format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(_handle.get(), format, &props);

        if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
    }
    CVK_STABLE_THROW(true, "format unsupported") {}
    return VK_FORMAT_MAX_ENUM;
}

auto PhysicalDevice::queueFamilyIndices(
    span<QueueFamilyProperties const> queueProperties
) const -> vector<queue_family_index_t> {
    debug_check(*this);

    if(queueProperties.empty())
        return {};

    vector<vector<queue_family_index_t>> queueIndices{queueProperties.size()};

    for(auto [requiredProperties, indices] : std::views::zip(queueProperties, queueIndices))
        for(auto const& family : _queueFamilies)
            if((family.properties & requiredProperties) == requiredProperties)
                indices.push_back(family.index);

    vector<size_t> familiesMaxQueues(_queueFamilies.size());

    for(auto [src, dst] : std::views::zip(_queueFamilies, familiesMaxQueues))
        dst = src.vkProperties.queueCount;

    return cth::alg::assign(queueIndices, familiesMaxQueues);
}

bool PhysicalDevice::supportsQueueSet(span<QueueFamilyProperties const> queueProperties) const {
    debug_check(*this);

    return !queueFamilyIndices(queueProperties).empty();
}

vector<VkPhysicalDevice> PhysicalDevice::enumerateVkDevices(vk_not_null<VkInstance> vkInstance) {
    Instance::debug_check_handle(vkInstance);

    uint32_t deviceCount = 0;
    auto const countResult = vkEnumeratePhysicalDevices(vkInstance.get(), &deviceCount, nullptr);
    CVK_RESULT_STABLE_THROW(countResult != VK_SUCCESS, countResult, "failed to count physical devices") {}

    vector<VkPhysicalDevice> devices(deviceCount);
    auto const writeResult = vkEnumeratePhysicalDevices(vkInstance.get(), &deviceCount, devices.data());
    CVK_RESULT_STABLE_THROW(writeResult != VK_SUCCESS, writeResult, "failed to enumerate physical devices") {}

    return devices;
}

vector<string> PhysicalDevice::queryExtensions(vk_not_null<VkPhysicalDevice> vkDevice) {
    uint32_t extensionCount = 0;
    auto const countResult = vkEnumerateDeviceExtensionProperties(vkDevice.get(), nullptr, &extensionCount, nullptr);
    CVK_RESULT_STABLE_THROW(countResult != VK_SUCCESS, countResult, "failed to count device extensions") {}

    vector<VkExtensionProperties> availableExtensions(extensionCount);
    auto const writeResult = vkEnumerateDeviceExtensionProperties(
        vkDevice.get(),
        nullptr,
        &extensionCount,
        availableExtensions.data()
    );
    CVK_RESULT_STABLE_THROW(
        writeResult != VK_SUCCESS,
        writeResult,
        "failed to enumerate device extension properties"
    ) {}

    vector<string> extensions(extensionCount);
    std::ranges::transform(
        availableExtensions,
        extensions.begin(),
        [](VkExtensionProperties const& extension) { return extension.extensionName; }
    );

    return extensions;
}

VkPhysicalDeviceProperties PhysicalDevice::queryProperties(vk_not_null<VkPhysicalDevice> vkDevice) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(vkDevice.get(), &properties);
    return properties;
}

VkPhysicalDeviceMemoryProperties PhysicalDevice::queryMemoryProperties(vk_not_null<VkPhysicalDevice> vkDevice) {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(vkDevice.get(), &memProperties);
    return memProperties;
}

vector<QueueFamily> PhysicalDevice::queryQueueFamilies(
    span<Surface const> surfaces,
    vk_not_null<VkPhysicalDevice> vkDevice
) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkDevice.get(), &count, nullptr);
    vector<VkQueueFamilyProperties> familyProperties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(vkDevice.get(), &count, familyProperties.data());

    vector<QueueFamily> queueFamilies{};
    queueFamilies.reserve(familyProperties.size());

    for(uint32_t i = 0; i < familyProperties.size(); i++) {
        bool presentSupport = !surfaces.empty();

        for(auto const& surface : surfaces) {
            VkBool32 support = VK_FALSE;
            auto const result = vkGetPhysicalDeviceSurfaceSupportKHR(vkDevice.get(), i, surface.get(), &support);
            CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "device-surface support query failed") {}

            presentSupport = presentSupport && support == VK_TRUE;
        }

        queueFamilies.push_back(QueueFamily::Vk(i, familyProperties[i], presentSupport));
    }

    return queueFamilies;
}

VkSampleCountFlagBits PhysicalDevice::evalMaxSampleCount(VkPhysicalDeviceProperties const& properties) {
    VkSampleCountFlags const supportedSamples = properties.limits.framebufferColorSampleCounts
        & properties.limits.framebufferDepthSampleCounts;

    constexpr std::array sampleFlagBits = {
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

QueueFamily const* PhysicalDevice::queueFamily(queue_family_index_t idx) const {
    auto const it = std::ranges::find_if(
        _queueFamilies,
        [idx](QueueFamily const& family) { return family.index == idx; }
    );

    if(it == _queueFamilies.end())
        return nullptr;

    return &*it;
}
}
