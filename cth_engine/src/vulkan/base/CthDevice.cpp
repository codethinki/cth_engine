#include "CthDevice.hpp"


#include "CthCore.hpp"
#include "CthInstance.hpp"
#include "CthPhysicalDevice.hpp"
#include "CthQueue.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


namespace cth::vk {
using std::vector;
using std::string_view;
using std::span;

Device::Device(const Instance* instance, const PhysicalDevice* physical_device, const std::span<Queue> queues) :
    _instance(instance),
    _physicalDevice(physical_device) {


    auto queueFamilyIndices = setUniqueFamilyIndices(queues);
    createLogicalDevice();
    wrapQueues(queueFamilyIndices, queues);
}
Device::~Device() {
    vkDestroyDevice(_handle.get(), nullptr);

    cth::log::msg<except::LOG>("destroyed device");
}

vector<uint32_t> Device::setUniqueFamilyIndices(const span<const Queue> queues) {
    const auto& queueFamilyIndices = _physicalDevice->queueFamilyIndices(queues);

    _familyIndices = queueFamilyIndices | std::ranges::to<std::unordered_set<uint32_t>>() | std::ranges::to<vector<uint32_t>>();

    return queueFamilyIndices;
}

void Device::createLogicalDevice() {
    vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    float queuePriority = 1.0f;
    std::ranges::for_each(_familyIndices, [&queueCreateInfos, queuePriority](const uint32_t queue_family) {
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
    std::ranges::transform(PhysicalDevice::REQUIRED_DEVICE_EXTENSIONS, deviceExtensions.begin(), [](const string_view ext) { return ext.data(); });

    createInfo.pEnabledFeatures = &PhysicalDevice::REQUIRED_DEVICE_FEATURES;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(PhysicalDevice::REQUIRED_DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice ptr = VK_NULL_HANDLE;

    const VkResult createResult = vkCreateDevice(_physicalDevice->get(), &createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create logical device")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    _handle = ptr;
}
void Device::wrapQueues(span<const uint32_t> family_indices, span<Queue> queues) const {
    auto queueCounts = family_indices | std::views::transform([](const uint32_t index) { return std::pair{index, 0}; }) | std::ranges::to<
        std::unordered_map<uint32_t, uint32_t>>();

    for(auto [index, queue] : std::views::zip(family_indices, queues)) {
        VkQueue ptr = VK_NULL_HANDLE;
        vkGetDeviceQueue(_handle.get(), index, queueCounts[index], &ptr);

        queue.wrap(index, queueCounts[index], ptr);
    }

}
void Device::waitIdle() const {
    const auto result = vkDeviceWaitIdle(_handle.get());

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to wait on device") throw except::vk_result_exception{result, details->exception()};
}


#ifdef CONSTANT_DEBUG_MODE
void Device::debug_check(const Device* device) {
    CTH_ERR(device == nullptr, "device must not be nullptr") throw details->exception();
    debug_check_handle(device->get());
}
void Device::debug_check_handle(const VkDevice vk_device) {
    CTH_ERR(vk_device == VK_NULL_HANDLE, "vk_device not be invalid (VK_NULL_HANDLE)") throw details->exception();
}
#endif

}
