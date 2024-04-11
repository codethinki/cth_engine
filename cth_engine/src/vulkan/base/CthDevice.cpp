#include "CthDevice.hpp"

#include <numeric>

#include "CthInstance.hpp"

#include "vulkan/pipeline/shader/CthShader.hpp"
#include "vulkan/surface/CthWindow.hpp"
#include "vulkan/utility/CthVkUtils.hpp"
#include "vulkan/surface/CthSurface.hpp"

#include <cth/cth_log.hpp>

#include "CthPhysicalDevice.hpp"



namespace cth {



Device::Device(PhysicalDevice* physical_device, Surface* surface, Instance* instance) : instance(instance), physicalDevice(physical_device) {

    createLogicalDevice(surface);
    createCommandPool();
}
Device::~Device() {
    vkDestroyCommandPool(vkDevice, commandPool, nullptr);

    vkDestroyDevice(vkDevice, nullptr);

    cth::log::msg<except::LOG>("destroyed device");
}



void Device::createLogicalDevice(const Surface* surface) {
    _queueIndices = physicalDevice->queueFamilyIndices(vector{VK_QUEUE_GRAPHICS_BIT}, surface);

    vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    float queuePriority = 1.0f;
    ranges::for_each(_queueIndices, [&queueCreateInfos, queuePriority](const uint32_t queue_family) {
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
    ranges::transform(PhysicalDevice::REQUIRED_DEVICE_EXTENSIONS, deviceExtensions.begin(), [](const string& ext) {
        return ext.c_str();
    });

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

    vkGetDeviceQueue(vkDevice, _queueIndices[GRAPHICS_QUEUE_INDEX], 0, &vkGraphicsQueue);
    vkGetDeviceQueue(vkDevice, _queueIndices[PRESENT_QUEUE_INDEX], 0, &vkPresentQueue);
}

void Device::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = _queueIndices[GRAPHICS_QUEUE_INDEX];
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;


    const VkResult createResult = vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &commandPool);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create command pool")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}


VkFormat Device::findSupportedFormat(const std::vector<VkFormat>& candidates, const VkImageTiling tiling,
    const VkFormatFeatureFlags features) const {
    for(const VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice->get(), format, &props);

        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
        if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
    }
    CTH_STABLE_ERR(true, "format unsupported") throw cth::except::data_exception{features, details->exception()};
}
uint32_t Device::findMemoryType(const uint32_t type_filter, const VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice->get(), &memProperties);

    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        if((type_filter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    CTH_STABLE_ERR(true, "no suitable memory type available") throw details->exception();
}

//---------------------------
// Buffer Helper Functions //BUG wtf is this move them to their appropriate classes
//---------------------------

VkCommandBuffer Device::beginSingleTimeCommands() const {
    //TODO make a setup phase and group these commands
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vkDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}
void Device::endSingleTimeCommands(VkCommandBuffer command_buffer) const {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;

    vkQueueSubmit(vkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vkGraphicsQueue);

    vkFreeCommandBuffers(vkDevice, commandPool, 1, &command_buffer);
}

void Device::createBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties,
    VkBuffer& buffer, VkDeviceMemory& buffer_memory) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VkResult createResult = vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &buffer);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create buffer")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vkDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    const VkResult allocResult = vkAllocateMemory(vkDevice, &allocInfo, nullptr, &buffer_memory);
    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate buffer memory")
        throw cth::except::vk_result_exception{allocResult, details->exception()};

    vkBindBufferMemory(vkDevice, buffer, buffer_memory, 0);
}
void Device::createImageWithInfo(const VkImageCreateInfo& image_info, const VkMemoryPropertyFlags properties, VkImage& image,
    VkDeviceMemory& image_memory) const {

    const VkResult createResult = vkCreateImage(vkDevice, &image_info, nullptr, &image);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create image")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vkDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    const VkResult allocResult = vkAllocateMemory(vkDevice, &allocInfo, nullptr, &image_memory);

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate image memory")
        throw cth::except::vk_result_exception{allocResult, details->exception()};

    const VkResult bindResult = vkBindImageMemory(vkDevice, image, image_memory, 0);
    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "failed to bind image memory")
        throw cth::except::vk_result_exception{bindResult, details->exception()};
}



}
