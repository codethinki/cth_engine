#include "CthPhysicalDevice.hpp"

#include "vulkan/base/CthInstance.hpp"
#include "vulkan/surface/CthSurface.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <ranges>
#include <set>

namespace cth {

PhysicalDevice::PhysicalDevice(VkPhysicalDevice device) : _vkDevice(device) { setConstants(); }
bool PhysicalDevice::suitable(const VkPhysicalDeviceFeatures& features, const span<const string_view> extensions,
    const Surface* surface, const span<const VkQueueFlagBits> queue_families) {

    const auto missingFeatures = supports(features);
    const auto missingExtensions = supports(extensions);
    const auto queueIndices = queueFamilyIndices(surface, queue_families);
    const bool valid = missingFeatures.empty() && missingExtensions.empty() && !queueIndices.empty();

    CTH_WARN(!valid, "physical device ({}) is missing features", _properties.deviceName) {
        for(const auto& missingFeature : missingFeatures) details->add("missing feature: {}", missingFeature);
        for(const auto& missingExtension : missingExtensions) details->add("missing extension: {}", missingExtension);
        if(queueIndices.empty()) details->add("missing queue families");
    }


    return valid;
}
vector<VkPhysicalDevice> PhysicalDevice::enumerateDevices(const Instance* instance) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance->get(), &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance->get(), &deviceCount, devices.data());
    return devices;
}


unique_ptr<PhysicalDevice> PhysicalDevice::autoPick(const VkPhysicalDeviceFeatures& features, const span<const string_view> extensions,
    const span<const VkQueueFlagBits> queue_families, const Surface* surface, const span<const VkPhysicalDevice> devices) {
    CTH_ERR(surface == nullptr, "surface must be a valid ptr")
        throw details->exception();

    vector<unique_ptr<PhysicalDevice>> physicalDevices{};
    physicalDevices.reserve(devices.size());
    ranges::transform(devices, back_inserter(physicalDevices), [](VkPhysicalDevice device) { return make_unique<PhysicalDevice>(device); });

    for(auto& physicalDevice : physicalDevices)
        if(physicalDevice->suitable(features, extensions, surface, queue_families)) {
            cth::log::msg<except::INFO>("chosen physical device: {}", physicalDevice->_properties.deviceName);
            return std::move(physicalDevice);
        }
    CTH_STABLE_ERR(true, "no GPU is suitable") throw details->exception();

}
unique_ptr<PhysicalDevice> PhysicalDevice::autoPick(const Surface* surface, const Instance* instance) {
    return autoPick(REQUIRED_DEVICE_FEATURES, REQUIRED_DEVICE_EXTENSIONS, vector{VK_QUEUE_GRAPHICS_BIT}, surface, enumerateDevices(instance));
}


vector<uint32_t> PhysicalDevice::supports(const VkPhysicalDeviceFeatures& required_features) {
    const auto availableFeatures = utils::deviceFeaturesToArray(_features);
    const auto requiredFeatures = utils::deviceFeaturesToArray(required_features);

    vector<uint32_t> missingFeatures{};

    for(uint32_t i = 0; i < availableFeatures.size(); i++) if(requiredFeatures[i] && !availableFeatures[i]) missingFeatures.push_back(i);


    return missingFeatures;
}
vector<string> PhysicalDevice::supports(span<const string_view> required_extensions) {
    vector<string> missingExtensions{};
    ranges::for_each(required_extensions, [&missingExtensions, this](string_view required_extension_name) {
        const bool missing = ranges::none_of(_extensions, [required_extension_name](const string_view extension) {
            return extension == required_extension_name;
        });

        if(missing) missingExtensions.emplace_back(required_extension_name);
    });
    return missingExtensions;
}

uint32_t PhysicalDevice::findMemoryType(const uint32_t type_filter, const VkMemoryPropertyFlags mem_properties) const {
    for(uint32_t i = 0; i < _memProperties.memoryTypeCount; i++)
        if((type_filter & (1 << i)) && (_memProperties.memoryTypes[i].propertyFlags & mem_properties) == mem_properties)
            return i;

    CTH_STABLE_ERR(true, "no suitable memory type available") throw details->exception();
}

VkFormat PhysicalDevice::findSupportedFormat(const span<const VkFormat> candidates, const VkImageTiling tiling,
    const VkFormatFeatureFlags features) const {
    for(const VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(_vkDevice, format, &props);

        if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
    }
    CTH_STABLE_ERR(true, "format unsupported") throw cth::except::data_exception{features, details->exception()};
}

vector<uint32_t> PhysicalDevice::queueFamilyIndices(const Surface* surface, const span<const VkQueueFlagBits> requested_queues) {
    const size_t queueCount = requested_queues.size() + (surface != nullptr ? 1 : 0);

    vector<VkQueueFlagBits> missingQueues{requested_queues.begin(), requested_queues.end()};
    vector<uint32_t> queueIndices{};
    queueIndices.reserve(queueCount);

    if(surface != nullptr) {
        auto i = 0u;
        for(; i < _queueFamilies.size() && !supportsPresentQueue(surface, i); ++i);

        CTH_STABLE_WARN(i == _queueFamilies.size(), "no present queue found");
        else queueIndices.push_back(i);
    }

    for(auto i = 0u; i < _queueFamilies.size() && !missingQueues.empty(); ++i)
        for(auto j = 0u; j < missingQueues.size() && !missingQueues.empty(); ++j)
            if(_queueFamilies[i].queueFlags & missingQueues[j]) {
                queueIndices.push_back(i);
                missingQueues.erase(missingQueues.begin() + j--);
            }

    CTH_WARN(queueIndices.size() != queueCount, "queues missing")
        for(const auto& missingQueue : missingQueues) details->add("missing queue: {}", static_cast<uint32_t>(missingQueue));


    return queueIndices.size() == queueCount ? queueIndices : vector<uint32_t>{};
}
bool PhysicalDevice::supportsPresentQueue(const Surface* surface, const uint32_t family_index) const {
    VkBool32 support;
    const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(_vkDevice, family_index, surface->get(), &support);
    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface support query failed")
        throw except::vk_result_exception{result, details->exception()};
    return support;
}


vector<VkPresentModeKHR> PhysicalDevice::supportedPresentModes(const Surface* surface) const {
    uint32_t size;
    const VkResult result1 = vkGetPhysicalDeviceSurfacePresentModesKHR(_vkDevice, surface->get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface present modes query failed")
        throw cth::except::vk_result_exception{result1, details->exception()};

    if(!size) return {};

    vector<VkPresentModeKHR> modes(size);
    const VkResult result2 = vkGetPhysicalDeviceSurfacePresentModesKHR(_vkDevice, surface->get(), &size, modes.data());

    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface present modes query failed")
        throw cth::except::vk_result_exception{result2, details->exception()};

    return modes;
}
vector<VkSurfaceFormatKHR> PhysicalDevice::supportedFormats(const Surface* surface) const {
    uint32_t size;
    const VkResult result1 = vkGetPhysicalDeviceSurfaceFormatsKHR(_vkDevice, surface->get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result1, details->exception()};

    if(!size) return {};
    vector<VkSurfaceFormatKHR> formats(size);
    const VkResult result2 = vkGetPhysicalDeviceSurfaceFormatsKHR(_vkDevice, surface->get(), &size, formats.data());
    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result2, details->exception()};

    return formats;
}
VkSurfaceCapabilitiesKHR PhysicalDevice::capabilities(const Surface* surface) const {
    VkSurfaceCapabilitiesKHR capabilities;
    const auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_vkDevice, surface->get(), &capabilities);

    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface capabilities query failed")
        throw except::vk_result_exception{result, details->exception()};

    return capabilities;
}
VkBool32 PhysicalDevice::supportsFamily(const Surface* physical_device, const uint32_t family_index) const {
    VkBool32 support;
    const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(_vkDevice, family_index, physical_device->get(), &support);
    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface support query failed")
        throw except::vk_result_exception{result, details->exception()};
    return support;

}



void PhysicalDevice::setFeatures() { vkGetPhysicalDeviceFeatures(_vkDevice, &_features); }
void PhysicalDevice::setExtensions() {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(_vkDevice, nullptr, &extensionCount, nullptr);
    vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(_vkDevice, nullptr, &extensionCount, availableExtensions.data());

    ranges::transform(availableExtensions, back_inserter(_extensions),
        [](const VkExtensionProperties& extension) { return extension.extensionName; });
}
void PhysicalDevice::setProperties() {
    vkGetPhysicalDeviceProperties(_vkDevice, &_properties);

    vkGetPhysicalDeviceMemoryProperties(_vkDevice, &_memProperties);

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_vkDevice, &count, nullptr);
    _queueFamilies.resize(count);
    vkGetPhysicalDeviceQueueFamilyProperties(_vkDevice, &count, _queueFamilies.data());
}
void PhysicalDevice::setMaxSampleCount() {
    const VkSampleCountFlags counts = _properties.limits.framebufferColorSampleCounts &
        _properties.limits.framebufferDepthSampleCounts;

    if(counts & VK_SAMPLE_COUNT_64_BIT) _maxSampleCount = VK_SAMPLE_COUNT_64_BIT;
    else if(counts & VK_SAMPLE_COUNT_32_BIT) _maxSampleCount = VK_SAMPLE_COUNT_32_BIT;
    else if(counts & VK_SAMPLE_COUNT_16_BIT) _maxSampleCount = VK_SAMPLE_COUNT_16_BIT;
    else if(counts & VK_SAMPLE_COUNT_8_BIT) _maxSampleCount = VK_SAMPLE_COUNT_8_BIT;
    else if(counts & VK_SAMPLE_COUNT_4_BIT) _maxSampleCount = VK_SAMPLE_COUNT_4_BIT;
    else if(counts & VK_SAMPLE_COUNT_2_BIT) _maxSampleCount = VK_SAMPLE_COUNT_2_BIT;

    else _maxSampleCount = VK_SAMPLE_COUNT_1_BIT;
}
void PhysicalDevice::setConstants() {
    setFeatures();
    setExtensions();
    setProperties();
    setMaxSampleCount();
}
}
