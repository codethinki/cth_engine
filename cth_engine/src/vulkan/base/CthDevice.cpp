#include "CthDevice.hpp"

#include <numeric>
#include <unordered_set>

#include "CthInstance.hpp"

#include "vulkan/surface/CthWindow.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include "CthPhysicalDevice.hpp"



namespace cth {



Device::Device(PhysicalDevice* physical_device, const Surface* surface, Instance* instance) : _instance(instance), _physicalDevice(physical_device) {
    setQueueIndices(surface);
    createLogicalDevice();
    setQueues();
}
Device::~Device() {
    vkDestroyDevice(_handle.get(), nullptr);

    cth::log::msg<except::LOG>("destroyed device");
}

void Device::setQueueIndices(const Surface* surface) {
    _queueIndices = _physicalDevice->queueFamilyIndices(surface, vector{VK_QUEUE_GRAPHICS_BIT});
    unordered_set<uint32_t> uniqueIndices{std::begin(_queueIndices), std::end(_queueIndices)};
    _uniqueQueueIndices.resize(uniqueIndices.size());
    ranges::copy(uniqueIndices, std::begin(_uniqueQueueIndices));
}

void Device::createLogicalDevice() {
    vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    float queuePriority = 1.0f;
    ranges::for_each(_uniqueQueueIndices, [&queueCreateInfos, queuePriority](const uint32_t queue_family) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queue_family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    });


    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    vector<const char*> deviceExtensions(PhysicalDevice::REQUIRED_DEVICE_EXTENSIONS.size());
    ranges::transform(PhysicalDevice::REQUIRED_DEVICE_EXTENSIONS, deviceExtensions.begin(), [](const string_view ext) { return ext.data(); });

    createInfo.pEnabledFeatures = &PhysicalDevice::REQUIRED_DEVICE_FEATURES;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(PhysicalDevice::REQUIRED_DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // might not really be necessary anymore because device specific validation layers
    // have been deprecated
    if constexpr(Constant::ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(Instance::VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = Instance::VALIDATION_LAYERS.data();
    } else createInfo.enabledLayerCount = 0;

    VkDevice ptr = VK_NULL_HANDLE;

    const VkResult createResult = vkCreateDevice(_physicalDevice->get(), &createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create logical device")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    _handle = ptr;
}
void Device::setQueues() {
    vkGetDeviceQueue(_handle.get(), _queueIndices[GRAPHICS_QUEUE_I], 0, &_vkGraphicsQueue);
    vkGetDeviceQueue(_handle.get(), _queueIndices[PRESENT_QUEUE_I], 0, &_vkPresentQueue);
}

#ifdef CONSTANT_DEBUG_MODE
void Device::debug_check(const Device* device) {
    CTH_ERR(device == nullptr, "device must not be nullptr") throw details->exception();
    CTH_ERR(device->get() == VK_NULL_HANDLE, "device must be created") throw details->exception();
}
#endif

}
