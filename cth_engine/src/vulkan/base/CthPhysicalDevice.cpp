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



std::optional<PhysicalDevice> PhysicalDevice::Create(VkPhysicalDevice device, Instance const* instance, Surface const& surface,
    std::span<Queue const> const queues, std::span<std::string const> const required_extensions,
    utils::PhysicalDeviceFeatures const& required_features) {

    PhysicalDevice physicalDevice(device, instance, surface, required_features, required_extensions);

    if(physicalDevice.suitable(queues)) return physicalDevice;
    return std::nullopt;


}


bool PhysicalDevice::suitable(std::span<Queue const> const queues) {

    auto const missingFeatures = supports(_requiredFeatures);
    auto const missingExtensions = supports(_requiredExtensions);
    auto const queueIndices = queueFamilyIndices(queues);
    bool const valid = missingFeatures.empty() && missingExtensions.empty() && !queueIndices.empty();

    CTH_ERR(!valid, "physical device ({}) is missing features", _properties.deviceName) {
        if(queueIndices.empty()) details->add("missing queue families");
        for(auto const& missingExtension : missingExtensions) details->add("missing extension: {}", missingExtension);
        for(auto const& missingFeature : missingFeatures)
            std::visit(cth::var::overload{
                [&details](size_t const index) { details->add("missing feature ({})", index); },
                [&details](VkStructureType const s_type) { details->add("missing feature2 extension: ({})", static_cast<uint32_t>(s_type)); }
            }, missingFeature);

        throw details->exception();
    }


    return valid;
}
vector<VkPhysicalDevice> PhysicalDevice::enumerateVkDevices(Instance const& instance) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance.get(), &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance.get(), &deviceCount, devices.data());
    return devices;
}

//TEMP left off here complete this function and add the required required_extensions / features to be used by the logical device. 
auto PhysicalDevice::AutoPick(Instance const* instance, std::span<Queue const> const queues,
    span<std::string const> const required_extensions, utils::PhysicalDeviceFeatures const& required_features) -> unique_ptr<PhysicalDevice> {
    auto const devices = enumerateVkDevices(*instance);

    auto joinView = ranges::views::concat(required_extensions, constants::REQUIRED_DEVICE_EXTENSIONS);
    vector<std::string> requiredExtensions{std::ranges::begin(joinView), std::ranges::end(joinView)};


    utils::PhysicalDeviceFeatures requiredFeatures{constants::REQUIRED_DEVICE_FEATURES2};
    requiredFeatures.merge(required_features);

    auto const surface = Surface::Temp(instance);

    vector<unique_ptr<PhysicalDevice>> physicalDevices{};
    for(auto& device : devices) {
        auto physicalDevice = Create(device, instance, surface, queues, requiredExtensions, requiredFeatures);
        if(physicalDevice.has_value()) physicalDevices.emplace_back(std::make_unique<PhysicalDevice>(std::move(physicalDevice.value())));
    }
    CTH_STABLE_ERR(physicalDevices.empty(), "no GPU is suitable") throw details->exception();

    cth::log::msg<except::INFO>("chosen physical device: {}", physicalDevices[0]->_properties.deviceName);

    return std::move(physicalDevices[0]);
}



auto PhysicalDevice::supports(utils::PhysicalDeviceFeatures const& required_features) const -> std::vector<std::variant<size_t, VkStructureType>> {
    return _features.supports(required_features);
}

auto PhysicalDevice::supports(std::span<std::string const> const required_extensions) -> vector<std::string> {
    vector<std::string> missingExtensions{};
    for(auto const& requiredExtension : required_extensions) {
        auto const it = std::ranges::find(_extensions, requiredExtension);

        if(it == _extensions.end()) missingExtensions.emplace_back(requiredExtension);
    }

    return missingExtensions;
}

uint32_t PhysicalDevice::findMemoryType(uint32_t const type_filter, VkMemoryPropertyFlags const mem_properties) const {
    for(uint32_t i = 0; i < _memProperties.memoryTypeCount; i++)
        if((type_filter & (1 << i)) && (_memProperties.memoryTypes[i].propertyFlags & mem_properties) == mem_properties)
            return i;

    CTH_STABLE_ERR(true, "no suitable memory type available") throw details->exception();
}

auto PhysicalDevice::findSupportedFormat(span<VkFormat const> const candidates, VkImageTiling const tiling,
    VkFormatFeatureFlags const features) const -> VkFormat {
    for(VkFormat const format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(_vkDevice.get(), format, &props);

        if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
    }
    CTH_STABLE_ERR(true, "format unsupported") throw except::data_exception{features, details->exception()};
}

auto PhysicalDevice::queueFamilyIndices(span<Queue const> queues) const -> vector<uint32_t> {
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

    auto const result = algorithm::assign(queueIndices, familiesMaxQueues); //TEMP left off here the assign doesnt work
    return result;
}
bool PhysicalDevice::supportsQueueSet(span<Queue const> const queues) const { return !queueFamilyIndices(queues).empty(); }


PhysicalDevice::PhysicalDevice(VkPhysicalDevice device, Instance const* instance, Surface const& surface,
    utils::PhysicalDeviceFeatures const& required_features, std::span<std::string const> required_extensions) :
    _vkDevice(device), _instance(instance),
    _features(device, required_features), _requiredFeatures(required_features),
    _requiredExtensions({required_extensions.begin(), required_extensions.end()}) { setConstants(surface); }

void PhysicalDevice::setExtensions() {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(_vkDevice.get(), nullptr, &extensionCount, nullptr);
    vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(_vkDevice.get(), nullptr, &extensionCount, availableExtensions.data());

    std::ranges::transform(availableExtensions, back_inserter(_extensions),
        [](VkExtensionProperties const& extension) { return extension.extensionName; });
}
void PhysicalDevice::setProperties() {
    vkGetPhysicalDeviceProperties(_vkDevice.get(), &_properties);

    vkGetPhysicalDeviceMemoryProperties(_vkDevice.get(), &_memProperties);
}
void PhysicalDevice::setQueueFamilyProperties(Surface const& surface) {
    vector<VkQueueFamilyProperties> familyProperties{};
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_vkDevice.get(), &count, nullptr);
    familyProperties.resize(count);
    vkGetPhysicalDeviceQueueFamilyProperties(_vkDevice.get(), &count, familyProperties.data());


    vector<bool> presentSupport(familyProperties.size());

    for(uint32_t i = 0; i < familyProperties.size(); i++) {
        VkBool32 support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(_vkDevice.get(), i, surface.get(), &support);
        presentSupport[i] = support;
    }

    _queueFamilies.reserve(presentSupport.size());
    for(size_t i = 0; i < presentSupport.size(); i++) _queueFamilies.emplace_back(static_cast<uint32_t>(i), familyProperties[i], presentSupport[i]);
}


void PhysicalDevice::setMaxSampleCount() {
    VkSampleCountFlags const counts = _properties.limits.framebufferColorSampleCounts &
        _properties.limits.framebufferDepthSampleCounts;

    if(counts & VK_SAMPLE_COUNT_64_BIT) _maxSampleCount = VK_SAMPLE_COUNT_64_BIT;
    else if(counts & VK_SAMPLE_COUNT_32_BIT) _maxSampleCount = VK_SAMPLE_COUNT_32_BIT;
    else if(counts & VK_SAMPLE_COUNT_16_BIT) _maxSampleCount = VK_SAMPLE_COUNT_16_BIT;
    else if(counts & VK_SAMPLE_COUNT_8_BIT) _maxSampleCount = VK_SAMPLE_COUNT_8_BIT;
    else if(counts & VK_SAMPLE_COUNT_4_BIT) _maxSampleCount = VK_SAMPLE_COUNT_4_BIT;
    else if(counts & VK_SAMPLE_COUNT_2_BIT) _maxSampleCount = VK_SAMPLE_COUNT_2_BIT;

    else _maxSampleCount = VK_SAMPLE_COUNT_1_BIT;
}
void PhysicalDevice::setConstants(Surface const& surface) {
    setExtensions();
    setProperties();
    setQueueFamilyProperties(surface);
    setMaxSampleCount();
}

#ifdef CONSTANT_DEBUG_MODE
void PhysicalDevice::debug_check(PhysicalDevice const* device) {
    CTH_ERR(device == nullptr, "physical device invalid (nullptr)") throw details->exception();
    debug_check_handle(device->get());
}
void PhysicalDevice::debug_check_handle(VkPhysicalDevice vk_device) {
    CTH_ERR(vk_device == VK_NULL_HANDLE, "physical device handle invalid (VK_NULL_HANDLE)")
        throw details->exception();
}
#endif


}
