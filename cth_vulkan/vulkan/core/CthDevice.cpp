#include "CthDevice.hpp"
#include "CthInstance.hpp"
#include "../utils/cth_vk_specific_utils.hpp"

#include <cth/cth_log.hpp>


#include <cstring>
#include <iostream>
#include <set>
#include <unordered_set>



namespace cth {


vector<uint32_t> Device::checkDeviceFeatureSupport(const VkPhysicalDevice& device) const {
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    const auto availableFeatures = deviceFeaturesToArray(features);
    const auto requiredFeatures = deviceFeaturesToArray(REQUIRED_DEVICE_FEATURES);

    vector<uint32_t> missingFeatures{};

    for(uint32_t i = 0; i < availableFeatures.size(); i++) if(requiredFeatures[i] && !availableFeatures[i]) missingFeatures.push_back(i);

    return missingFeatures;
}
VkSampleCountFlagBits Device::evaluateMaxUsableSampleCount() const {
    const VkSampleCountFlags counts = physicalProperties.limits.framebufferColorSampleCounts &
        physicalProperties.limits.framebufferDepthSampleCounts;
    if(counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if(counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
    if(counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if(counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
    if(counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if(counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;

    return VK_SAMPLE_COUNT_1_BIT;
}

//create surface
void Device::createSurface() { window->createWindowSurface(instance->get(), &windowSurface); }


//pickPhysicalDevice helpers
QueueFamilyIndices Device::findQueueFamilies(const VkPhysicalDevice device) const {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    QueueFamilyIndices indices;
    vector<uint32_t> graphicFamilies{};
    vector<uint32_t> presentFamilies{};



    for(auto [i, queueFamily] : queueFamilies | views::enumerate) {
        if(queueFamily.queueCount == 0) continue;

        if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicFamilies.push_back(i);

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, windowSurface, &presentSupport);
        if(presentSupport) presentFamilies.push_back(i);
    }

    //iterate through all possible combinations of graphic and present indices
    for(const auto graphicFamily : graphicFamilies) {
        indices.graphicsFamilyIndex = graphicFamily;
        for(const auto presentFamily : presentFamilies) {
            indices.presentFamilyIndex = presentFamily;
            if(graphicFamily == presentFamily) continue;
            if(indices.complete()) return indices;
        }
    }

    return indices;
}
vector<string> Device::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    vector<string> missingExtensions{};
    ranges::for_each(REQUIRED_DEVICE_EXTENSIONS, [&missingExtensions, availableExtensions](string_view required_extension_name) {
        const bool missing = ranges::none_of(availableExtensions, [required_extension_name](const VkExtensionProperties& available_extension) {
            return available_extension.extensionName == required_extension_name;
        });
        if(missing) missingExtensions.emplace_back(required_extension_name);
    });
    return missingExtensions;
}
SwapchainSupportDetails Device::querySwapchainSupport(const VkPhysicalDevice device) const {
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, windowSurface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, windowSurface, &formatCount, nullptr);

    if(formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, windowSurface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, windowSurface, &presentModeCount, nullptr);

    if(presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, windowSurface, &presentModeCount, details.presentModes.data());
    }
    return details;
}
bool Device::physicalDeviceSuitable(VkPhysicalDevice physical_device) const {
    string details{};
    bool suitable = true;

    const QueueFamilyIndices indices = findQueueFamilies(physical_device);
    if(!(suitable = indices.complete())) {
        details += "\n required queue family not supported\n";
        if(!indices.graphicsFamily()) details += "\tgraphics queue family missing\n";
        if(!indices.presentFamily()) details += "\tpresent queue family missing\n";
    }

    auto missingExtensions = checkDeviceExtensionSupport(physical_device);
    if(!(suitable = missingExtensions.empty())) {
        details += "\n missing device extensions:\n";
        ranges::for_each(missingExtensions, [&details](const string_view extension) { details += '\t' + string(extension) + '\n'; });
    }

    const SwapchainSupportDetails swapChainSupport = querySwapchainSupport(physical_device);
    const bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    if(!(suitable = swapChainAdequate)) {
        details += "\n swapchain capabilities insufficient:";
        if(swapChainSupport.formats.empty()) details += "\tno swap chain formats available\n";
        if(swapChainSupport.presentModes.empty()) details += "\tno swap chain present modes available\n";
    }

    const auto missingFeatures = checkDeviceFeatureSupport(physical_device);
    if(!(suitable = missingFeatures.empty())) {
        details += "\n missing device features:\n";
        ranges::for_each(missingFeatures,
            [&details](const uint32_t feature) { details += '\t' + string(deviceFeatureIndexToString(feature)) + '\n'; });
    }

    if(!suitable) cth::log::msg(cth::except::INFO, "device {} not suitable\n{}", physicalProperties.deviceName, details);
    return suitable;
}

void Device::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance->get(), &deviceCount, nullptr);
    CTH_STABLE_ERR(deviceCount == 0, "no vulkan GPUs found") throw details->exception();

    cth::log::msg(cth::except::INFO, "device count: {}", deviceCount);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance->get(), &deviceCount, devices.data());

    //TODO implement proper scoring
    const auto deviceIt = ranges::find_if(devices, [this](const VkPhysicalDevice& device) { return physicalDeviceSuitable(device); });
    physicalDevice = *deviceIt;

    CTH_STABLE_ERR(deviceIt == devices.end(), "no GPU is suitable") throw details->exception();


    vkGetPhysicalDeviceProperties(physicalDevice, &physicalProperties);

    cth::log::msg(cth::except::INFO, "chosen physical device: {}", physicalProperties.deviceName);
}



void Device::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    const vector<uint32_t> uniqueQueueFamilies = {indices.graphicsFamilyIndex, indices.presentFamilyIndex};
    vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    float queuePriority = 1.0f;
    ranges::for_each(uniqueQueueFamilies, [this, &queueCreateInfos, queuePriority](const uint32_t queue_family) {
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

    createInfo.pEnabledFeatures = &REQUIRED_DEVICE_FEATURES;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data();

    // might not really be necessary anymore because device specific validation layers
    // have been deprecated
    if constexpr(Instance::ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = Instance::VALIDATION_LAYERS.size();
        createInfo.ppEnabledLayerNames = Instance::VALIDATION_LAYERS.data();
    } else createInfo.enabledLayerCount = 0;


    const VkResult createResult = vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "VK: failed to create logical device")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    vkGetDeviceQueue(logicalDevice, indices.graphicsFamilyIndex, 0, &graphicsQueue);
    vkGetDeviceQueue(logicalDevice, indices.presentFamilyIndex, 0, &presentQueue);
}

void Device::createCommandPool() {
    const QueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;


    const VkResult createResult = vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "VK: failed to create command pool")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}
void Device::initShaders() {
    //TODO create a shader manager so it is not in the device class


    vertShader = make_unique<Shader>(VERTEX_SHADER_CODE_PATH, VERTEX_SHADER_BINARIES_PATH, SHADER_COMPILER_PATH);
    fragShader = make_unique<Shader>(FRAGMENT_SHADER_CODE_PATH, FRAGMENT_SHADER_BINARIES_PATH, SHADER_COMPILER_PATH);

#ifdef _DEBUG
    const auto vertResult = vertShader->compile();
    const auto fragResult = fragShader->compile();
    CTH_ERR(!vertResult.empty(), "vertex shader glsl compiling failed") {
        ranges::for_each(vertResult, [&details](const string& str) { details->add(str); });
        throw details->exception();
    }
    CTH_ERR(!fragResult.empty(), "fragment shader glsl compiling failed") {
        ranges::for_each(vertResult, [&details](const string& str) { details->add(str); });
        throw details->exception();
    }
#endif
    vertShader->loadSpv();
    vertShader->createModule(device());
    fragShader->loadSpv();
    fragShader->createModule(device());

}



VkFormat Device::findSupportedFormat(const std::vector<VkFormat>& candidates, const VkImageTiling tiling,
    const VkFormatFeatureFlags features) const {
    for(const VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
        if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
    }
    CTH_STABLE_ERR(true, "format unsupported") throw cth::except::data_exception{features, details->exception()};
}
uint32_t Device::findMemoryType(const uint32_t type_filter, const VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        if((type_filter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    CTH_STABLE_ERR(true, "no suitable memory type available") throw details->exception();
}

//---------------------------
// Buffer Helper Functions
//---------------------------

VkCommandBuffer Device::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

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

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &command_buffer);
}

void Device::createBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties,
    VkBuffer& buffer, VkDeviceMemory& buffer_memory) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VkResult createResult = vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "Vk: failed to create buffer")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    const VkResult allocResult = vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &buffer_memory);
    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "Vk: failed to allocate buffer memory")
        throw cth::except::vk_result_exception{allocResult, details->exception()};

    vkBindBufferMemory(logicalDevice, buffer, buffer_memory, 0);
}
void Device::copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, const VkDeviceSize size, const VkDeviceSize src_offset,
    const VkDeviceSize dst_offset) const {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = src_offset;
    copyRegion.dstOffset = dst_offset;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src_buffer, dst_buffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}
void Device::copyBufferToImage(VkBuffer buffer, VkImage image, const uint32_t width, const uint32_t height,
    const uint32_t layer_count) const {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = layer_count;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(commandBuffer);
}
void Device::createImageWithInfo(const VkImageCreateInfo& image_info, const VkMemoryPropertyFlags properties, VkImage& image,
    VkDeviceMemory& image_memory) const {

    const VkResult createResult = vkCreateImage(logicalDevice, &image_info, nullptr, &image);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "Vk: failed to create image") throw cth::except::vk_result_exception{createResult,
        details->exception()};

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    const VkResult allocResult = vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &image_memory);

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "Vk: failed to allocate image memory")
        throw cth::except::vk_result_exception{allocResult, details->exception()};

    const VkResult bindResult = vkBindImageMemory(logicalDevice, image, image_memory, 0);
    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "Vk: failed to bind image memory")
        throw cth::except::vk_result_exception{bindResult, details->exception()};
}

Device::Device(Window* window, Instance* instance) : window(window), instance(instance) {
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    initShaders();
}
Device::~Device() {
    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);
}


}
