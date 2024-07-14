#include "CthPhysicalDevice.hpp"

#include "CthQueue.hpp"
#include "vulkan/base/CthInstance.hpp"
#include "vulkan/surface/CthSurface.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


namespace cth {
using std::vector;
using std::string_view;
using std::span;
using std::unique_ptr;


PhysicalDevice::PhysicalDevice(VkPhysicalDevice device, const Instance* instance, const Surface& surface) : _vkDevice(device), _instance(instance) {
    setConstants(surface);
}
bool PhysicalDevice::suitable(const VkPhysicalDeviceFeatures& features, const std::span<const std::string_view> extensions,
    const std::span<const Queue> queues) {

    const auto missingFeatures = supports(features);
    const auto missingExtensions = supports(extensions);
    const auto queueIndices = queueFamilyIndices(queues);
    const bool valid = missingFeatures.empty() && missingExtensions.empty() && !queueIndices.empty();

    CTH_ERR(!valid, "physical device ({}) is missing features", _properties.deviceName) {
        for(const auto& missingFeature : missingFeatures) details->add("missing feature: {}", missingFeature);
        for(const auto& missingExtension : missingExtensions) details->add("missing extension: {}", missingExtension);
        if(queueIndices.empty()) details->add("missing queue families");
        throw details->exception();
    }


    return valid;
}
vector<VkPhysicalDevice> PhysicalDevice::enumerateDevices(const Instance& instance) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance.get(), &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance.get(), &deviceCount, devices.data());
    return devices;
}


unique_ptr<PhysicalDevice> PhysicalDevice::autoPick(const Instance* instance, const VkPhysicalDeviceFeatures& features,
    const span<const std::string_view> extensions,
    span<const VkPhysicalDevice> devices, const std::span<const Queue> queues) {
    const auto surface = Surface::Temp(instance);


    vector<unique_ptr<PhysicalDevice>> physicalDevices{};
    physicalDevices.reserve(devices.size());
    std::ranges::transform(devices, back_inserter(physicalDevices), [instance, &surface](VkPhysicalDevice device) {
        return std::make_unique<PhysicalDevice>(device, instance, surface);
    });

    for(auto& physicalDevice : physicalDevices)
        if(physicalDevice->suitable(features, extensions, queues)) {
            cth::log::msg<except::INFO>("chosen physical device: {}", physicalDevice->_properties.deviceName);
            return std::move(physicalDevice);
        }
    CTH_STABLE_ERR(true, "no GPU is suitable") throw details->exception();

}
unique_ptr<PhysicalDevice> PhysicalDevice::autoPick(const Instance& instance, const std::span<const Queue> queues) {
    return autoPick(&instance, REQUIRED_DEVICE_FEATURES, REQUIRED_DEVICE_EXTENSIONS, enumerateDevices(instance), queues);
}



vector<uint32_t> PhysicalDevice::supports(const VkPhysicalDeviceFeatures& required_features) const {
    const auto availableFeatures = utils::deviceFeaturesToArray(_features);
    const auto requiredFeatures = utils::deviceFeaturesToArray(required_features);

    vector<uint32_t> missingFeatures{};

    for(uint32_t i = 0; i < availableFeatures.size(); i++) if(requiredFeatures[i] && !availableFeatures[i]) missingFeatures.push_back(i);


    return missingFeatures;
}
vector<std::string> PhysicalDevice::supports(const span<const string_view> required_extensions) {
    vector<std::string> missingExtensions{};
    for(const auto& requiredExtension : required_extensions) {
        const auto it = std::ranges::find(_extensions, requiredExtension);

        if(it == _extensions.end()) missingExtensions.emplace_back(requiredExtension);
    }

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
        vkGetPhysicalDeviceFormatProperties(_vkDevice.get(), format, &props);

        if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
    }
    CTH_STABLE_ERR(true, "format unsupported") throw except::data_exception{features, details->exception()};
}

vector<uint32_t> PhysicalDevice::queueFamilyIndices(span<const Queue> queues) const {
    vector<vector<uint32_t>> queueIndices(queues.size());

    for(auto [queue, indices] : std::views::zip(queues, queueIndices)) {
        const auto requiredProperties = queue.familyProperties();
        for(uint32_t index = 0; index < _queueFamilies.size(); ++index) {
            const bool support = (_queueFamilies[index].properties & requiredProperties) == requiredProperties;
            if(support) indices.push_back(index);
        }
    }

    const auto familiesMaxQueues = _queueFamilies | std::views::transform([](const QueueFamily& family) { return family.vkProperties.queueCount; }) |
        std::ranges::to<vector<size_t>>();

    const auto result = algorithm::assign(queueIndices, familiesMaxQueues); //TEMP left off here the assign doesnt work
    return result;
}
bool PhysicalDevice::supportsQueueSet(const span<const Queue> queues) const { return !queueFamilyIndices(queues).empty(); }



void PhysicalDevice::setFeatures() { vkGetPhysicalDeviceFeatures(_vkDevice.get(), &_features); }
void PhysicalDevice::setExtensions() {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(_vkDevice.get(), nullptr, &extensionCount, nullptr);
    vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(_vkDevice.get(), nullptr, &extensionCount, availableExtensions.data());

    std::ranges::transform(availableExtensions, back_inserter(_extensions),
        [](const VkExtensionProperties& extension) { return extension.extensionName; });
}
void PhysicalDevice::setProperties() {
    vkGetPhysicalDeviceProperties(_vkDevice.get(), &_properties);

    vkGetPhysicalDeviceMemoryProperties(_vkDevice.get(), &_memProperties);
}
void PhysicalDevice::setQueueFamilyProperties(const Surface& surface) {
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
void PhysicalDevice::setConstants(const Surface& surface) {
    setFeatures();
    setExtensions();
    setProperties();
    setQueueFamilyProperties(surface);
    setMaxSampleCount();
}

#ifdef CONSTANT_DEBUG_MODE
void PhysicalDevice::debug_check(const PhysicalDevice* device) {
    CTH_ERR(device == nullptr, "physical device invalid (nullptr)") throw details->exception();
    debug_check_handle(device->get());
}
void PhysicalDevice::debug_check_handle(VkPhysicalDevice vk_device) {
    CTH_ERR(vk_device == VK_NULL_HANDLE, "physical device handle invalid (VK_NULL_HANDLE)")
        throw details->exception();
}
#endif


}
