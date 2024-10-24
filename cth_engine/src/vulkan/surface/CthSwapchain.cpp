#include "CthSwapchain.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/surface/CthWindow.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <array>
#include <limits>



namespace cth {
//public
VkImageView Swapchain::createImageView(const VkDevice& device, VkImage image, const VkFormat format, const VkImageAspectFlags aspect_flags,
    const uint32_t mip_levels) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect_flags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mip_levels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView imageView;
    if(vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) throw runtime_error("createImageView: failed to create image view");
    return imageView;
}

VkFormat Swapchain::findDepthFormat() const {
    return device->findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkResult Swapchain::acquireNextImage(uint32_t* image_index) const {
    vkWaitForFences(device->get(), 1, &inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

    const VkResult result = vkAcquireNextImageKHR(device->get(), vkSwapchain, std::numeric_limits<uint64_t>::max(),
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, image_index);

    return result;
}


VkResult Swapchain::submit(VkCommandBuffer command_buffer) const {
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = 1u;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];

    constexpr array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = 1u;
    submitInfo.pCommandBuffers = &command_buffer;

    submitInfo.signalSemaphoreCount = 1u;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

    return vkQueueSubmit(device->graphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]);
}
VkResult Swapchain::present(const uint32_t image_index) const {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];

    const array<VkSwapchainKHR, 1> swapchains{vkSwapchain};

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains.data();
    presentInfo.pImageIndices = &image_index;

    return vkQueuePresentKHR(device->presentQueue(), &presentInfo);
}

VkResult Swapchain::submitCommandBuffer(VkCommandBuffer buffer, const uint32_t image_index) {
    if(imagesInFlight[image_index] != VK_NULL_HANDLE) vkWaitForFences(device->get(), 1, &imagesInFlight[image_index], VK_TRUE, UINT64_MAX);

    imagesInFlight[image_index] = inFlightFences[currentFrame];

    vkResetFences(device->get(), 1, &inFlightFences[currentFrame]);


    const VkResult submitResult = submit(buffer);
    CTH_STABLE_ERR(submitResult != VK_SUCCESS, "failed to submit draw call")
        throw cth::except::vk_result_exception{submitResult, details->exception()}; //TEMP this is bad structure

    const auto presentResult = present(image_index);

    ++currentFrame %= MAX_FRAMES_IN_FLIGHT;
    return presentResult;
}



VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) {
    const auto it = ranges::find_if(available_formats, [](const VkSurfaceFormatKHR& available_format) {
        return available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });

    CTH_STABLE_WARN(it == available_formats.end(), "no suitable format found choosing [0]") return available_formats[0];

    return *it;
}
VkPresentModeKHR Swapchain::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& available_present_modes) {
    const auto it = ranges::find(available_present_modes, VK_PRESENT_MODE_MAILBOX_KHR);

    if(it == available_present_modes.end()) {
        cth::log::msg<except::INFO>("present mode: FIFO (V-Sync)");
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    cth::log::msg<except::INFO>("present mode: MAILBOX");
    return VK_PRESENT_MODE_MAILBOX_KHR;

    //VK_PRESENT_MODE_IMMEDIATE_KHR
}
VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;

    VkExtent2D actualExtent = windowExtent;

    actualExtent.width = std::max(capabilities.minImageExtent.width,
        std::min(capabilities.maxImageExtent.width, actualExtent.width));

    actualExtent.height = std::max(capabilities.minImageExtent.height,
        std::min(capabilities.maxImageExtent.height, actualExtent.height));

    return actualExtent;
}

void Swapchain::createSwapchain() {
    const SwapchainSupportDetails swapchainSupport = device->getSwapchainSupport();

    const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
    const VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
    const VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if(swapchainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapchainSupport.capabilities.maxImageCount)
        imageCount = swapchainSupport.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = window->surface();

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const auto [graphicsFamilyIndex, presentFamilyIndex] = device->findPhysicalQueueFamilies();
    const array<uint32_t, 2> queueFamilyIndices{graphicsFamilyIndex, presentFamilyIndex};

    if(graphicsFamilyIndex != presentFamilyIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = oldSwapchain == nullptr ? VK_NULL_HANDLE : oldSwapchain->vkSwapchain;

    const VkResult createResult = vkCreateSwapchainKHR(device->get(), &createInfo, nullptr, &vkSwapchain);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create swapchain")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
}
void Swapchain::createImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for(size_t i = 0; i < swapchainImages.size(); i++)
        swapchainImageViews[i] = createImageView(device->get(), swapchainImages[i], swapchainImageFormat);
}
void Swapchain::setImageCount() {
    // we only specified a minimum number of images in the swap chain, so the implementation is
    // allowed to create a swap chain with more. That's why we'll first query the final number of
    // images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
    // retrieve the handles.
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device->get(), vkSwapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device->get(), vkSwapchain, &imageCount, swapchainImages.data());
}
void Swapchain::setMsaaSampleCount() { msaaSamples = evaluateMsaaSampleCount(); }

VkAttachmentDescription Swapchain::createColorAttachmentDescription() const {
    VkAttachmentDescription attachment = {};
    attachment.format = getSwapchainImageFormat();
    attachment.samples = msaaSamples;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    return attachment;
}
VkAttachmentDescription Swapchain::createDepthAttachment() const {
    VkAttachmentDescription attachment{};
    attachment.format = findDepthFormat();
    attachment.samples = msaaSamples;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    return attachment;
}
VkAttachmentDescription Swapchain::createColorAttachmentResolve() const {
    VkAttachmentDescription attachment{};
    attachment.format = swapchainImageFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    return attachment;
}
SubpassDescription Swapchain::createSubpassDescription() const {
    constexpr VkAttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    constexpr VkAttachmentReference depthAttachmentRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    constexpr VkAttachmentReference colorAttachmentResolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    //TEMP left off here this doesn't work

    return SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, vector{colorAttachmentRef}, depthAttachmentRef, vector{colorAttachmentResolveRef});
}
VkSubpassDependency Swapchain::createSubpassDependency() const {
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcAccessMask = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstSubpass = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    return dependency;
}

void Swapchain::createRenderPass() {
    const auto colorAttachment = createColorAttachmentDescription();
    const auto depthAttachment = createDepthAttachment();
    const auto colorAttachmentResolve = createColorAttachmentResolve();


    auto subpassDescription = createSubpassDescription();

    const auto subpassDependency = createSubpassDependency();

    const array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = subpassDescription.ptr();
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &subpassDependency;

    const VkResult createResult = vkCreateRenderPass(device->get(), &renderPassInfo, nullptr, &renderPass);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create render pass")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}


VkImageCreateInfo Swapchain::createColorImageInfo() const {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapchainExtent.width;
    imageInfo.extent.height = swapchainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = swapchainImageFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.samples = msaaSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;

    return imageInfo;
}
VkImageCreateInfo Swapchain::createDepthImageInfo(VkFormat depth_format) const {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapchainExtent.width;
    imageInfo.extent.height = swapchainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depth_format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = msaaSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;

    return imageInfo;
}
void Swapchain::createColorResources() {
    //TODO maybe use the image class here

    msaaImages.resize(imageCount());
    msaaImageMemories.resize(imageCount());
    msaaImageViews.resize(imageCount());


    const auto imageInfo = createColorImageInfo();
    for(uint32_t i = 0; i < msaaImages.size(); i++) {
        device->createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, msaaImages[i], msaaImageMemories[i]);

        msaaImageViews[i] = createImageView(device->get(), msaaImages[i], swapchainImageFormat);
    }
}

void Swapchain::createDepthResources() {
    const VkFormat depthFormat = findDepthFormat();
    swapchainImageFormat = depthFormat;

    depthImages.resize(imageCount());
    depthImageMemories.resize(imageCount());
    depthImageViews.resize(imageCount());

    const auto imageInfo = createDepthImageInfo(depthFormat);

    for(uint32_t i = 0; i < depthImages.size(); i++) {
        device->createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depthImages[i], depthImageMemories[i]);

        depthImageViews[i] = createImageView(device->get(), depthImages[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
}


void Swapchain::createFramebuffers() {
    swapchainFramebuffers.resize(imageCount());

    for(size_t i = 0; i < imageCount(); i++) {
        array<VkImageView, 3> attachments = {msaaImageViews[i], depthImageViews[i], swapchainImageViews[i]};

        const VkExtent2D swapchainExtent = getSwapchainExtent();
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;

        const VkResult createResult = vkCreateFramebuffer(device->get(), &framebufferInfo, nullptr, &swapchainFramebuffers[i]);

        CTH_STABLE_ERR(createResult != VK_SUCCESS, "Vk: failed to create framebuffer")
            throw cth::except::vk_result_exception{createResult, details->exception()};
    }
}

void Swapchain::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        const VkResult createWaitSemaphoreResult = vkCreateSemaphore(device->get(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
        const VkResult createSignalSemaphoreResult = vkCreateSemaphore(device->get(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        const VkResult createFenceResult = vkCreateFence(device->get(), &fenceInfo, nullptr, &inFlightFences[i]);

        CTH_STABLE_ERR(!(createWaitSemaphoreResult == createSignalSemaphoreResult ==
            createFenceResult == VK_SUCCESS), "Vk: failed to create synchronization objects") {
            if(createWaitSemaphoreResult != VK_SUCCESS) details->add("wait semaphore creation failed");
            if(createSignalSemaphoreResult != VK_SUCCESS) details->add("signal semaphore creation failed");
            if(createFenceResult != VK_SUCCESS) details->add("fence creation failed");

            throw cth::except::vk_result_exception{static_cast<VkResult>(createWaitSemaphoreResult | createSignalSemaphoreResult | createFenceResult),
                details->exception()};
        }
    }
}


void Swapchain::init() {
    createSwapchain();
    setImageCount();
    createImageViews();

    setMsaaSampleCount();
    createRenderPass();
    createColorResources();
    createDepthResources();

    createFramebuffers();
    createSyncObjects();
}



Swapchain::Swapchain(Device* device, Window* window, const VkExtent2D window_extent) : device(device), window(window), windowExtent{window_extent} {
    init();
}
Swapchain::Swapchain(Device* device, Window* window, const VkExtent2D window_extent, shared_ptr<Swapchain> previous) : device{device}, window(window),
    windowExtent{window_extent}, oldSwapchain{std::move(previous)} {
    init();
    oldSwapchain = nullptr;
}
Swapchain::~Swapchain() {
    ranges::for_each(swapchainImageViews, [this](VkImageView image_view) { vkDestroyImageView(device->get(), image_view, nullptr); });
    swapchainImageViews.clear();

    if(vkSwapchain != nullptr) {
        vkDestroySwapchainKHR(device->get(), vkSwapchain, nullptr);
        vkSwapchain = nullptr;
    }

    for(uint32_t i = 0; i < depthImages.size(); i++) {
        vkDestroyImageView(device->get(), depthImageViews[i], nullptr);
        vkDestroyImage(device->get(), depthImages[i], nullptr);
        vkFreeMemory(device->get(), depthImageMemories[i], nullptr);
    }

    for(uint32_t i = 0; i < msaaImages.size(); i++) {
        vkDestroyImageView(device->get(), msaaImageViews[i], nullptr);
        vkDestroyImage(device->get(), msaaImages[i], nullptr);
        vkFreeMemory(device->get(), msaaImageMemories[i], nullptr);
    }

    ranges::for_each(swapchainFramebuffers, [this](VkFramebuffer framebuffer) { vkDestroyFramebuffer(device->get(), framebuffer, nullptr); });

    vkDestroyRenderPass(device->get(), renderPass, nullptr);

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device->get(), renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device->get(), imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device->get(), inFlightFences[i], nullptr);
    }
}



VkSampleCountFlagBits Swapchain::evaluateMsaaSampleCount() const {
    const uint32_t maxSamples = device->evaluateMaxUsableSampleCount(); //TEMP

    uint32_t samples = 1;
    while(samples < maxSamples && samples < MAX_MSAA_SAMPLES) samples *= 2;

    return static_cast<VkSampleCountFlagBits>(samples);
}



}
