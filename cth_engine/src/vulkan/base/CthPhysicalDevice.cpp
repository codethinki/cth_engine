#include "CthPhysicalDevice.hpp"

#include "CthQueue.hpp"
#include "vulkan/base/CthInstance.hpp"
#include "vulkan/surface/CthSurface.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


namespace cth::vk {
using std::vector;
using std::string_view;
using std::span;
using std::unique_ptr;



PhysicalDevice::PhysicalDevice(cth::not_null<Instance const*> instance, utils::PhysicalDeviceFeatures required_features,
    std::span<std::string const> required_extensions) :
    _instance{(instance.get())}, _requiredFeatures{std::move(required_features)},
    _requiredExtensions{std::from_range, required_extensions} {}

PhysicalDevice::PhysicalDevice(cth::not_null<Instance const*> instance, utils::PhysicalDeviceFeatures const& required_features,
    std::span<std::string const> required_extensions, Surface const& surface, vk::not_null<VkPhysicalDevice> vk_device) : PhysicalDevice{instance,
    required_features, required_extensions} { create(surface, vk_device); }

PhysicalDevice::PhysicalDevice(cth::not_null<Instance const*> instance, utils::PhysicalDeviceFeatures const& required_features,
    std::span<std::string const> required_extensions, State const& state) : PhysicalDevice{instance, required_features,
    required_extensions} { wrap(state); }


std::optional<PhysicalDevice> PhysicalDevice::Create(cth::not_null<Instance const*> instance, Surface const& surface,
    std::span<Queue const> queues, std::span<std::string const> required_extensions, utils::PhysicalDeviceFeatures const& required_features,
    vk::not_null<VkPhysicalDevice> vk_device) {

    PhysicalDevice physicalDevice{instance, required_features, required_extensions, surface, vk_device};

    if(physicalDevice.suitable(queues)) return physicalDevice;
    return std::nullopt;
}

void PhysicalDevice::wrap(State const& state) {
    auto const device = state.vkDevice;
    DEBUG_CHECK_PHYSICAL_DEVICE_HANDLE(device);

    _handle = device.get();

    if(!state.features.empty()) _features = state.features;
    else _features = utils::PhysicalDeviceFeatures{device, _requiredFeatures};

    if(!state.extensions.empty()) _extensions = state.extensions;
    else _extensions = getExtensions(device);

    _properties = state.properties.value_or(getProperties(device));

    if(_maxSampleCount != VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM) _maxSampleCount = state.maxSampleCount;
    else _maxSampleCount = evalMaxSampleCount(_properties);


    _memProperties = state.memProperties.value_or(getMemoryProperties(device));
    _queueFamilies = state.queueFamilies;

}
void PhysicalDevice::create(Surface const& surface, cth::not_null<VkPhysicalDevice> vk_device) {
    DEBUG_CHECK_PHYSICAL_DEVICE_HANDLE(vk_device);

    _handle = vk_device.get();
    _features = utils::PhysicalDeviceFeatures{vk_device, _requiredFeatures};
    _extensions = getExtensions(vk_device);
    _properties = getProperties(vk_device);
    _maxSampleCount = evalMaxSampleCount(_properties);
    _memProperties = getMemoryProperties(vk_device);
    _queueFamilies = getQueueFamilies(surface, vk_device);
}


bool PhysicalDevice::suitable(std::span<Queue const> queues) {
    DEBUG_CHECK_PHYSICAL_DEVICE(this);

    auto const missingFeatures = supports(_requiredFeatures);
    auto const missingExtensions = supports(_requiredExtensions);
    auto const queueIndices = queueFamilyIndices(queues);
    bool const valid = missingFeatures.empty() && missingExtensions.empty() && !queueIndices.empty();

    CTH_ERR(!valid, "physical device ({}) is missing features", _properties.deviceName) {
        if(queueIndices.empty()) details->add("missing queue families");
        for(auto const& missingExtension : missingExtensions) details->add("missing extension: {}", missingExtension);
        for(auto const& missingFeature : missingFeatures)
            std::visit(cth::var::overload{
                [&details](size_t index) { details->add("missing feature ({})", index); },
                [&details](VkStructureType s_type) { details->add("missing feature2 extension: ({})", static_cast<uint32_t>(s_type)); }
            }, missingFeature);

        throw details->exception();
    }


    return valid;
}


auto PhysicalDevice::AutoPick(cth::not_null<Instance const*> instance, std::span<Queue const> queues, span<std::string const> required_extensions,
    utils::PhysicalDeviceFeatures const& required_features) -> unique_ptr<PhysicalDevice> {
    DEBUG_CHECK_INSTANCE(instance);

    auto const devices = enumerateVkDevices(instance->get());

    auto joinView = ranges::views::concat(required_extensions, constants::REQUIRED_DEVICE_EXTENSIONS);
    vector<std::string> requiredExtensions{std::ranges::begin(joinView), std::ranges::end(joinView)};


    utils::PhysicalDeviceFeatures requiredFeatures{constants::REQUIRED_DEVICE_FEATURES2};
    requiredFeatures.merge(required_features);

    auto const surface = Surface::Temp(instance);

    vector<unique_ptr<PhysicalDevice>> physicalDevices{};
    for(auto& device : devices) {
        auto physicalDevice = Create(instance, surface, queues, requiredExtensions, requiredFeatures, device);
        if(physicalDevice.has_value()) physicalDevices.emplace_back(std::make_unique<PhysicalDevice>(std::move(physicalDevice.value())));
    }
    CTH_STABLE_ERR(physicalDevices.empty(), "no GPU is suitable") throw details->exception();

    cth::log::msg<except::INFO>("chosen physical device: {}", physicalDevices[0]->_properties.deviceName);

    return std::move(physicalDevices[0]);
}



auto PhysicalDevice::supports(utils::PhysicalDeviceFeatures const& required_features) const -> std::vector<std::variant<size_t, VkStructureType>> {
    DEBUG_CHECK_PHYSICAL_DEVICE(this);

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
    DEBUG_CHECK_PHYSICAL_DEVICE(this);

    for(uint32_t i = 0; i < _memProperties.memoryTypeCount; i++)
        if((type_filter & (1 << i)) && (_memProperties.memoryTypes[i].propertyFlags & mem_properties) == mem_properties)
            return i;

    CTH_STABLE_ERR(true, "no suitable memory type available") throw details->exception();
}

auto PhysicalDevice::findSupportedFormat(span<VkFormat const> candidates, VkImageTiling tiling,
    VkFormatFeatureFlags features) const -> VkFormat {
    DEBUG_CHECK_PHYSICAL_DEVICE(this);

    for(VkFormat const format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(_handle.get(), format, &props);

        if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
    }
    CTH_STABLE_ERR(true, "format unsupported") throw except::data_exception{features, details->exception()};
}

auto PhysicalDevice::queueFamilyIndices(span<Queue const> queues) const -> vector<uint32_t> {
    DEBUG_CHECK_PHYSICAL_DEVICE(this);

    vector<vector<uint32_t>> queueIndices(queues.size());

    for(auto [queue, indices] : std::views::zip(queues, queueIndices)) {
        auto const requiredProperties = queue.familyProperties();
        for(uint32_t index = 0; index < _queueFamilies.size(); ++index) {
            bool const support = (_queueFamilies[index].properties & requiredProperties) == requiredProperties;
            if(support) indices.push_back(index);
        }
    }

    auto const familiesMaxQueues = _queueFamilies | std::views::transform([](QueueFamily const& family) { return family.vkProperties.queueCount; }) |
        std::ranges::to<vector<size_t>>();

    auto const result = algorithm::assign(queueIndices, familiesMaxQueues);
    return result;
}
bool PhysicalDevice::supportsQueueSet(span<Queue const> queues) const {
    DEBUG_CHECK_PHYSICAL_DEVICE(this);

    return !queueFamilyIndices(queues).empty();
}

vector<VkPhysicalDevice> PhysicalDevice::enumerateVkDevices(vk::not_null<VkInstance> vk_instance) {
    DEBUG_CHECK_INSTANCE_HANDLE(vk_instance);

    uint32_t deviceCount = 0;
    auto const countResult = vkEnumeratePhysicalDevices(vk_instance.get(), &deviceCount, nullptr);
    CTH_STABLE_ERR(countResult != VK_SUCCESS, "failed to count physical devices") throw result_exception{countResult, details->exception()};

    std::vector<VkPhysicalDevice> devices(deviceCount);
    auto const writeResult = vkEnumeratePhysicalDevices(vk_instance.get(), &deviceCount, devices.data());
    CTH_STABLE_ERR(writeResult != VK_SUCCESS, "failed to write physical devices") throw result_exception{writeResult, details->exception()};

    return devices;
}

vector<std::string> PhysicalDevice::getExtensions(vk::not_null<VkPhysicalDevice> vk_device) {
    uint32_t extensionCount = 0;
    auto const countResult = vkEnumerateDeviceExtensionProperties(vk_device.get(), nullptr, &extensionCount, nullptr);
    CTH_STABLE_ERR(countResult != VK_SUCCESS, "failed to count device extensions") throw result_exception{countResult, details->exception()};

    vector<VkExtensionProperties> availableExtensions{extensionCount};
    auto const writeResult = vkEnumerateDeviceExtensionProperties(vk_device.get(), nullptr, &extensionCount, availableExtensions.data());
    CTH_STABLE_ERR(writeResult != VK_SUCCESS, "failed to write device extension properties")
        throw result_exception
            {writeResult, details->exception()};

    vector<std::string> extensions{extensionCount};

    std::ranges::transform(availableExtensions, extensions.begin(), [](VkExtensionProperties const& extension) { return extension.extensionName; });

    return extensions;
}
VkPhysicalDeviceProperties PhysicalDevice::getProperties(vk::not_null<VkPhysicalDevice> vk_device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(vk_device.get(), &properties);
    return properties;
}
VkPhysicalDeviceMemoryProperties PhysicalDevice::getMemoryProperties(vk::not_null<VkPhysicalDevice> vk_device) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vk_device.get(), &memProperties);
    return memProperties;
}
std::vector<QueueFamily> PhysicalDevice::getQueueFamilies(Surface const& surface, vk::not_null<VkPhysicalDevice> vk_device) {
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
    for(size_t i = 0; i < presentSupport.size(); i++) queueFamilies.emplace_back(static_cast<uint32_t>(i), familyProperties[i], presentSupport[i]);

    return queueFamilies;
}
VkSampleCountFlagBits PhysicalDevice::evalMaxSampleCount(VkPhysicalDeviceProperties const& properties) {
    VkSampleCountFlags const counts = properties.limits.framebufferColorSampleCounts &
        properties.limits.framebufferDepthSampleCounts;

    VkSampleCountFlagBits maxSampleCount;
    if(counts & VK_SAMPLE_COUNT_64_BIT) maxSampleCount = VK_SAMPLE_COUNT_64_BIT;
    else if(counts & VK_SAMPLE_COUNT_32_BIT) maxSampleCount = VK_SAMPLE_COUNT_32_BIT;
    else if(counts & VK_SAMPLE_COUNT_16_BIT) maxSampleCount = VK_SAMPLE_COUNT_16_BIT;
    else if(counts & VK_SAMPLE_COUNT_8_BIT) maxSampleCount = VK_SAMPLE_COUNT_8_BIT;
    else if(counts & VK_SAMPLE_COUNT_4_BIT) maxSampleCount = VK_SAMPLE_COUNT_4_BIT;
    else if(counts & VK_SAMPLE_COUNT_2_BIT) maxSampleCount = VK_SAMPLE_COUNT_2_BIT;
    else maxSampleCount = VK_SAMPLE_COUNT_1_BIT;

    return maxSampleCount;
}


#ifdef CONSTANT_DEBUG_MODE
void PhysicalDevice::debug_check(cth::not_null<PhysicalDevice const*> device) {
    CTH_ERR(!device->created(), "physical device must be created") throw details->exception();
    debug_check_handle(device->get());
}
void PhysicalDevice::debug_check_handle([[maybe_unused]] vk::not_null<VkPhysicalDevice> vk_device) {}
#endif


}
