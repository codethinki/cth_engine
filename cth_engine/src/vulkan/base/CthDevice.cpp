#include "CthDevice.hpp"

#include <numeric>
#include <unordered_set>

#include "CthInstance.hpp"

#include "vulkan/surface/CthWindow.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include "CthPhysicalDevice.hpp"



namespace cth {



Device::Device(PhysicalDevice* physical_device, const Surface* surface, Instance* instance) : instance(instance), physicalDevice(physical_device) {
    setQueueIndices(surface);
    createLogicalDevice();
    setQueues();
}
Device::~Device() {
    vkDestroyCommandPool(vkDevice, commandPool, nullptr);

    vkDestroyDevice(vkDevice, nullptr);

    cth::log::msg<except::LOG>("destroyed device");
}

void Device::setQueueIndices(const Surface* surface) {
    queueIndices_ = physicalDevice->queueFamilyIndices(surface, vector{VK_QUEUE_GRAPHICS_BIT});
    unordered_set<uint32_t> uniqueIndices{std::begin(queueIndices_), std::end(queueIndices_)};
    uniqueQueueIndices_.resize(uniqueIndices.size());
    ranges::copy(uniqueIndices, std::begin(uniqueQueueIndices_));
}

void Device::createLogicalDevice() {
    vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    float queuePriority = 1.0f;
    ranges::for_each(uniqueQueueIndices_, [&queueCreateInfos, queuePriority](const uint32_t queue_family) {
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
    if constexpr(Instance::ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(Instance::VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = Instance::VALIDATION_LAYERS.data();
    } else createInfo.enabledLayerCount = 0;


    const VkResult createResult = vkCreateDevice(physicalDevice->get(), &createInfo, nullptr, &vkDevice);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create logical device")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}
void Device::setQueues() {
    vkGetDeviceQueue(vkDevice, queueIndices_[GRAPHICS_QUEUE_I], 0, &vkGraphicsQueue);
    vkGetDeviceQueue(vkDevice, queueIndices_[PRESENT_QUEUE_I], 0, &vkPresentQueue);
}

#ifdef _DEBUG
void Device::debug_check(const Device* device) {
    CTH_ERR(device == nullptr, "device must not be nullptr") throw details->exception();
    CTH_ERR(device->get() == VK_NULL_HANDLE, "device must be created") throw details->exception();
}
#endif


//---------------------------
// Buffer Helper Functions //BUG wtf is this move them to their appropriate classes
//---------------------------
//TEMP clean this up
//VkCommandBuffer Device::beginSingleTimeCommands() const {
//    //TODO make a setup phase and group these commands
//    VkCommandBufferAllocateInfo allocInfo{};
//    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
//    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//    allocInfo.commandPool = commandPool;
//    allocInfo.commandBufferCount = 1;
//
//
//    VkCommandBuffer commandBuffer;
//    vkAllocateCommandBuffers(vkDevice, &allocInfo, &commandBuffer);
//
//    VkCommandBufferBeginInfo beginInfo{};
//    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//
//    vkBeginCommandBuffer(commandBuffer, &beginInfo);
//    return commandBuffer;
//}
//void Device::endSingleTimeCommands(VkCommandBuffer command_buffer) const {
//    vkEndCommandBuffer(command_buffer);
//
//    VkSubmitInfo submitInfo{};
//    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//    submitInfo.commandBufferCount = 1;
//    submitInfo.pCommandBuffers = &command_buffer;
//
//    vkQueueSubmit(vkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
//    vkQueueWaitIdle(vkGraphicsQueue);
//
//    vkFreeCommandBuffers(vkDevice, commandPool, 1, &command_buffer);
//}



}
