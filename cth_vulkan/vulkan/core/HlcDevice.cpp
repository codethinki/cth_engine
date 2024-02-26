#include "HlcDevice.hpp"
#include "../utils/cth_vk_specific_utils.hpp"

#include <cth/cth_log.hpp>


#include <cstring>
#include <iostream>
#include <set>
#include <unordered_set>

#include <Windows.h>



const std::filesystem::path SHADER_COMPILER_PATH = L"E:/visual_studio/SDK/Vulkan/Bin/glslc.exe";
const std::filesystem::path VERTEX_SHADER_CODE_PATH = L"resources/shaders/vertShader.vert";
const std::filesystem::path FRAGMENT_SHADER_CODE_PATH = L"resources/shaders/fragShader.frag";
const std::filesystem::path VERTEX_SHADER_BINARIES_PATH = L"resources/shaders/vertShader.spv";
const std::filesystem::path FRAGMENT_SHADER_BINARIES_PATH = L"resources/shaders/fragShader.spv";


namespace cth {


string Device::checkDeviceFeatures(const VkPhysicalDevice& device) const {
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    auto availableFeatures = deviceFeaturesToArray(features);
    auto requiredFeatures = deviceFeaturesToArray(requiredFeatures);

    auto range = ranges::views::zip(requiredFeatures, availableFeatures) | ranges::views::enumerate | ranges::views::filter([](const auto& pair) {
        return !pair.first || (pair.first && pair.second);
    });

    string missingFeatures{};
    ranges::for_each(range, [&missingFeatures](const tuple<int, pair<bool, bool>>& results) {
        if(missingFeatures.empty()) missingFeatures = "missing features:\n";
        missingFeatures += '\t';
        missingFeatures += deviceFeatureIndexToString(get<0>(results));
        missingFeatures += '\n';
    });

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

//createInstance helpers

vector<const char*> Device::getRequiredExtensions() const {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if constexpr(enableValidationLayers) extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}


//create surface
void Device::createSurface() { window.createWindowSurface(instance, &windowSurface); }


//pickPhysicalDevice helpers
QueueFamilyIndices Device::findQueueFamilies(const VkPhysicalDevice device) const {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    QueueFamilyIndices indices;
    for(auto [i, queueFamily] : queueFamilies | views::enumerate) {
        if(queueFamily.queueCount == 0) continue;

        if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            indices.graphicsFamilyHasValue = true;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, windowSurface, &presentSupport);
        if(presentSupport) {
            indices.presentFamily = i;
            indices.presentFamilyHasValue = true;
        }

        if(indices.isComplete()) break;
    }

    return indices;
}
string Device::checkDeviceExtensionSupport(const VkPhysicalDevice device) const {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    set<string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for(const auto& [extensionName, specVersion] : availableExtensions) requiredExtensions.erase(extensionName);

    if(requiredExtensions.empty()) return {};

    string missingExtensions = "missing device extensions:\n";
    ranges::for_each(requiredExtensions, [&missingExtensions](const string& extension) { missingExtensions += '\t' + extension + '\n'; });
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
string Device::isDeviceSuitable(VkPhysicalDevice device) const {
    string details{};

    QueueFamilyIndices indices = findQueueFamilies(device);
    if(!indices.isComplete()) {
        details += "required queue family not supported\n";
        if(!indices.graphicsFamilyHasValue) details += "\tgraphics queue family missing\n";
        if(!indices.presentFamilyHasValue) details += "\tpresent queue family missing\n";
    }

    details += checkDeviceExtensionSupport(device);

    const SwapchainSupportDetails swapChainSupport = querySwapchainSupport(device);
    const bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    if(!swapChainAdequate) {
        details += "swapchain capabilities insufficient:";
        if(swapChainSupport.formats.empty()) details += "\tno swap chain formats available\n";
        if(swapChainSupport.presentModes.empty()) details += "\tno swap chain present modes available\n";
    }

    details += checkDeviceFeatures(physicalDevice);


    return details;
}

void Device::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    CTH_STABLE_ERR(deviceCount != 0, "no vulkan GPUs found") throw details->exception();

    cth::log::msg(cth::except::INFO, "device count: {}", deviceCount);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    //BUG implement proper device selection and error handling
    const auto deviceIt = ranges::find_if(devices, [this](const VkPhysicalDevice& device) { return isDeviceSuitable(device); });
    physicalDevice = *deviceIt;

    CTH_STABLE_ERR(deviceIt != devices.end(), "no GPU is suitable") throw details->exception();


    vkGetPhysicalDeviceProperties(physicalDevice, &physicalProperties);

    cth::log::msg(cth::except::INFO, "physical device: {}", physicalProperties.deviceName);
}



void Device::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

    float queuePriority = 1.0f;
    for(uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }


    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &requiredDeviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // might not really be necessary anymore because device specific validation layers
    // have been deprecated
    if constexpr(enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else createInfo.enabledLayerCount = 0;


    const VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice);
    CTH_STABLE_ERR(result == VK_SUCCESS, "VK: failed to create logical device") throw cth::except::data_exception{result, details->exception()};

    vkGetDeviceQueue(logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(logicalDevice, indices.presentFamily, 0, &presentQueue);
}
void Device::createCommandPool() {
    const QueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;


    const VkResult result = vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool);
    CTH_STABLE_ERR(result == VK_SUCCESS, "VK: failed to create command pool") throw cth::except::data_exception{result, details->exception()};
}
void Device::initShaders() {
    vertShader = make_unique<Shader>(VERTEX_SHADER_CODE_PATH, VERTEX_SHADER_BINARIES_PATH, SHADER_COMPILER_PATH);
    fragShader = make_unique<Shader>(FRAGMENT_SHADER_CODE_PATH, FRAGMENT_SHADER_BINARIES_PATH, SHADER_COMPILER_PATH);

#ifdef _DEBUG
    const auto vertResult = vertShader->compile();
    const auto fragResult = fragShader->compile();
    CTH_ERR(vertResult.empty(), "vertex shader glsl compiling failed") throw cth::except::data_exception{vertResult, details->exception()};
    CTH_ERR(fragResult.empty(), "fragment shader glsl compiling failed") throw cth::except::data_exception{fragResult, details->exception()};
#endif
    vertShader->loadSpv();
    vertShader->createModule(device());
    fragShader->loadSpv();
    fragShader->createModule(device());

    //TEMP does this have to be with the device?
}



VkFormat Device::findSupportedFormat(const std::vector<VkFormat>& candidates, const VkImageTiling tiling,
    const VkFormatFeatureFlags features) const {
    for(const VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) { return format; } else if(
            tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) { return format; }
    }
    throw std::runtime_error("failed to find supported format!");
}
uint32_t Device::findMemoryType(const uint32_t type_filter, const VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if((type_filter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) { return i; }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}


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
void Device::endSingleTimeCommands(const VkCommandBuffer command_buffer) const {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &command_buffer);
}

void Device::createBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage,
    const VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) { throw std::runtime_error("failed to create vertex buffer!"); }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &buffer_memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    vkBindBufferMemory(logicalDevice, buffer, buffer_memory, 0);
}
void Device::copyBuffer(const VkBuffer src_buffer, const VkBuffer dst_buffer, const VkDeviceSize size) const {
    const VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src_buffer, dst_buffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}
void Device::copyBufferToImage(const VkBuffer buffer, const VkImage image, const uint32_t width, const uint32_t height,
    const uint32_t layer_count) const {
    const VkCommandBuffer commandBuffer = beginSingleTimeCommands();

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

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);
    endSingleTimeCommands(commandBuffer);
}
void Device::createImageWithInfo(const VkImageCreateInfo& image_info, const VkMemoryPropertyFlags properties, VkImage& image,
    VkDeviceMemory& image_memory) const {

    if(const VkResult result = vkCreateImage(logicalDevice, &image_info, nullptr, &image); result != VK_SUCCESS) {
        int x = 0;
        throw runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &image_memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    if(vkBindImageMemory(logicalDevice, image, image_memory, 0) != VK_SUCCESS) throw runtime_error("failed to bind image memory!");
}

Device::Device(Window& window) : window{window} {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    initShaders();
}
Device::~Device() {
    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);

    if(enableValidationLayers) dev::destroyDebugUtilsMessengerExt(instance, debugMessenger, nullptr);

    vkDestroySurfaceKHR(instance, windowSurface, nullptr);
}



string hasGLFWRequiredInstanceExtensions() {

    cth::log::msg(cth::except::INFO, "available extensions: {}", getAvailableInstanceExtensions());

    uint32_t glfwExtensionCount = 0;
    const auto rawRequiredExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    vector<const char*> requiredExtensions{rawRequiredExtensions, rawRequiredExtensions + glfwExtensionCount};

    cth::log::msg(cth::except::INFO, "required extensions: {}", requiredExtensions.size());

    string missingExtensions{};
    ranges::for_each(requiredExtensions, [availableExtensions, &missingExtensions](const string_view required) {
        cth::log::msg(cth::except::INFO, "\t{}", required);

        if(ranges::none_of(availableExtensions, [required](const VkExtensionProperties& available_extension) {
            return available_extension.extensionName == required;
        })) {
            if(missingExtensions.empty()) missingExtensions = "missing GLFW required extensions:\n";
            missingExtensions += '\t';
            missingExtensions += required;
            missingExtensions += '\n';
        }
    });

    return missingExtensions;
}


} // namespace cth::dev
