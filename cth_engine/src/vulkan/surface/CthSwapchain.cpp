#include "CthSwapchain.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/pipeline/CthPipelineBarrier.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/resource/image/CthImage.hpp"
#include "vulkan/surface/CthSurface.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <array>
#include <limits>



namespace cth {

Swapchain::Swapchain(Device* device, DeletionQueue* deletion_queue, const VkExtent2D window_extent, const Surface* surface) : _device(device),
    _windowExtent(window_extent) { init(surface, deletion_queue); }
Swapchain::Swapchain(Device* device, DeletionQueue* deletion_queue, const VkExtent2D window_extent, const Surface* surface,
    shared_ptr<Swapchain> previous) : _device{device},
    _windowExtent(window_extent), _oldSwapchain{std::move(previous)} {
    init(surface, deletion_queue);
    _oldSwapchain = nullptr;
}
Swapchain::~Swapchain() {

    if(_vkSwapchain != nullptr) {
        vkDestroySwapchainKHR(_device->get(), _vkSwapchain, nullptr);
        _vkSwapchain = nullptr;
    }

    _swapchainImages.clear();
    _msaaImages.clear();
    _msaaImageViews.clear();
    _depthImages.clear();
    _depthImageViews.clear();


    ranges::for_each(_swapchainFramebuffers, [this](VkFramebuffer framebuffer) { vkDestroyFramebuffer(_device->get(), framebuffer, nullptr); });

    vkDestroyRenderPass(_device->get(), _renderPass, nullptr);

    for(size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(_device->get(), _renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(_device->get(), _imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(_device->get(), _inFlightFences[i], nullptr);
    }
}


VkResult Swapchain::acquireNextImage(uint32_t* image_index) const {
    vkWaitForFences(_device->get(), 1, &_inFlightFences[_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

    const VkResult result = vkAcquireNextImageKHR(_device->get(), _vkSwapchain, std::numeric_limits<uint64_t>::max(),
        _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, image_index);

    return result;
}
//TEMP remove the deletion queue from here and this entire function into the renderer
VkResult Swapchain::submitCommandBuffer(DeletionQueue* deletion_queue, const PrimaryCmdBuffer* cmd_buffer, const uint32_t image_index) {


    if(_imagesInFlight[image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(_device->get(), 1, &_imagesInFlight[image_index], VK_TRUE, UINT64_MAX);
        deletion_queue->clear(static_cast<uint32_t>(_currentFrame)); //TEMP remove this from here
    }
    deletion_queue->next(static_cast<uint32_t>(_currentFrame)); //TEMP remove this from here

    _imagesInFlight[image_index] = _inFlightFences[_currentFrame];

    vkResetFences(_device->get(), 1, &_inFlightFences[_currentFrame]);


    const VkResult submitResult = submit(vector{cmd_buffer});
    CTH_STABLE_ERR(submitResult != VK_SUCCESS, "failed to submit draw call")
        throw cth::except::vk_result_exception{submitResult, details->exception()};

    const auto presentResult = present(image_index);

    ++_currentFrame %= Constants::MAX_FRAMES_IN_FLIGHT;
    return presentResult;
}
void Swapchain::changeSwapchainImageQueue(const uint32_t release_queue, const CmdBuffer* release_cmd_buffer, const uint32_t acquire_queue,
    const CmdBuffer* acquire_cmd_buffer, const uint32_t image_index) {

    ImageBarrier releaseBarrier{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        {{_swapchainImages[image_index].get(), ImageBarrier::Info::QueueTransition(0, release_queue, 0, acquire_queue)}}
    };
    releaseBarrier.execute(*release_cmd_buffer);

    ImageBarrier barrier{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        {{_swapchainImages[image_index].get(), ImageBarrier::Info::QueueTransition(0, release_queue, 0, acquire_queue)}}
    };
    barrier.execute(*acquire_cmd_buffer);
}


VkResult Swapchain::submit(vector<const PrimaryCmdBuffer*> cmd_buffers) const {
    vector<VkCommandBuffer> cmdBuffers(cmd_buffers.size());
    ranges::transform(cmd_buffers, cmdBuffers.begin(), [](const PrimaryCmdBuffer* cmd_buffer) { return cmd_buffer->get(); });

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = 1u;
    submitInfo.pWaitSemaphores = &_imageAvailableSemaphores[_currentFrame];

    constexpr array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());
    submitInfo.pCommandBuffers = cmdBuffers.data();

    submitInfo.signalSemaphoreCount = 1u;
    submitInfo.pSignalSemaphores = &_renderFinishedSemaphores[_currentFrame];

    return vkQueueSubmit(_device->graphicsQueue(), 1, &submitInfo, _inFlightFences[_currentFrame]);
}
VkResult Swapchain::present(const uint32_t image_index) const {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &_renderFinishedSemaphores[_currentFrame];

    const array<VkSwapchainKHR, 1> swapchains{_vkSwapchain};

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains.data();
    presentInfo.pImageIndices = &image_index;

    return vkQueuePresentKHR(_device->presentQueue(), &presentInfo);
}

VkSampleCountFlagBits Swapchain::evalMsaaSampleCount() const {
    const uint32_t maxSamples = _device->physical()->maxSampleCount() / 2; //TEMP

    uint32_t samples = 1;
    while(samples < maxSamples && samples < Constants::MAX_MSAA_SAMPLES) samples *= 2;

    return static_cast<VkSampleCountFlagBits>(samples);
}

VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) {
    const auto it = ranges::find_if(available_formats, [](const VkSurfaceFormatKHR& available_format) {
        return available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });

    CTH_STABLE_WARN(it == available_formats.end(), "no suitable format found, choosing [0]") return available_formats[0];

    return *it;
}
VkPresentModeKHR Swapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) {
    const auto it = ranges::find(available_present_modes, VK_PRESENT_MODE_MAILBOX_KHR);

    /* if(it != available_present_modes.end()) {
        cth::log::msg<except::INFO>("present mode: MAILBOX");
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }*/

    //VK_PRESENT_MODE_IMMEDIATE_KHR


    cth::log::msg<except::INFO>("present mode: FIFO (V-Sync)");
    return VK_PRESENT_MODE_FIFO_KHR;

}
VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;

    VkExtent2D actualExtent = _windowExtent;

    actualExtent.width = std::max(capabilities.minImageExtent.width,
        std::min(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height = std::max(capabilities.minImageExtent.height,
        std::min(capabilities.maxImageExtent.height, actualExtent.height));

    return actualExtent;
}
uint32_t Swapchain::evalMinImageCount(const uint32_t min, const uint32_t max) const {
    uint32_t imageCount = min + 1; //TODO check if this is really wrong if the imageCount is 4
    if(max > 0 && imageCount > max) imageCount = max;
    return imageCount;
}

void Swapchain::createSwapchain(const Surface* surface) {

    const auto surfaceFormats = _device->physical()->supportedFormats(surface);
    const auto presentModes = _device->physical()->supportedPresentModes(surface);
    const auto capabilities = _device->physical()->capabilities(surface);

    const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(surfaceFormats);
    const VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
    const VkExtent2D extent = chooseSwapExtent(capabilities);

    const uint32_t imageCount = evalMinImageCount(capabilities.minImageCount, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface->get();

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0; // Optional
    createInfo.pQueueFamilyIndices = nullptr; // Optional


    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = _oldSwapchain == nullptr ? VK_NULL_HANDLE : _oldSwapchain->_vkSwapchain;

    const VkResult createResult = vkCreateSwapchainKHR(_device->get(), &createInfo, nullptr, &_vkSwapchain);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create swapchain")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    _imageFormat = surfaceFormat.format;
    _extent = extent;
}

BasicImage::Config Swapchain::createImageConfig() const {
    BasicImage::Config config;
    config.samples = _msaaSamples;

    config.aspectMask = VK_IMAGE_ASPECT_NONE;
    config.format = VK_FORMAT_UNDEFINED;
    config.usage = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;


    config.mipLevels = 1;
    config.tiling = VK_IMAGE_TILING_OPTIMAL;

    return config;
}
BasicImage::Config Swapchain::createColorImageConfig() const {
    auto config = createImageConfig();
    config.format = _imageFormat;
    config.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    config.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    return config;
}
BasicImage::Config Swapchain::createDepthImageConfig() const {
    auto config = createImageConfig();
    config.format = _depthFormat;
    config.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    config.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    return config;
}

void Swapchain::createSwapchainImages() {
    uint32_t imageCount; //only min specified, might be higher
    vkGetSwapchainImagesKHR(_device->get(), _vkSwapchain, &imageCount, nullptr);

    vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(_device->get(), _vkSwapchain, &imageCount, images.data());

    const auto imageConfig = createColorImageConfig();

    _swapchainImages.reserve(imageCount);
    for(auto image : images) _swapchainImages.emplace_back(make_unique<BasicImage>(_device, _extent, imageConfig, image, BasicImage::State::Default()));
    _swapchainImageViews.reserve(imageCount);
    for(auto i = 0u; i < imageCount; i++) _swapchainImageViews.emplace_back(_device, _swapchainImages[i].get(), ImageView::Config::Default());
}
void Swapchain::createMsaaResources(DeletionQueue* deletion_queue) {
    const auto imageConfig = createColorImageConfig();

    _msaaImages.reserve(imageCount());
    _msaaImageViews.reserve(imageCount());

    for(uint32_t i = 0; i < imageCount(); i++) {
        _msaaImages.emplace_back(make_unique<Image>(_device, deletion_queue, _extent, imageConfig, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        _msaaImageViews.emplace_back(_device, _msaaImages.back().get(), ImageView::Config::Default());
    }
}
VkFormat Swapchain::findDepthFormat() const {
    return _device->physical()->findSupportedFormat(
        vector{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
void Swapchain::createDepthResources(DeletionQueue* deletion_queue) {
    _depthFormat = findDepthFormat();

    const auto imageConfig = createDepthImageConfig();

    _depthImages.reserve(imageCount());
    _depthImageViews.reserve(imageCount());

    for(uint32_t i = 0; i < imageCount(); i++) {
        _depthImages.emplace_back(make_unique<Image>(_device, deletion_queue, _extent, imageConfig, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        _depthImageViews.emplace_back(_device, _depthImages.back().get(), ImageView::Config::Default());
    }
}



VkAttachmentDescription Swapchain::createColorAttachmentDescription() const {
    VkAttachmentDescription attachment = {};
    attachment.format = _imageFormat;
    attachment.samples = _msaaSamples;
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
    attachment.format = _depthFormat;
    attachment.samples = _msaaSamples;
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
    attachment.format = _imageFormat;
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

    const VkResult createResult = vkCreateRenderPass(_device->get(), &renderPassInfo, nullptr, &_renderPass);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create render pass")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}


void Swapchain::createFramebuffers() {
    _swapchainFramebuffers.resize(imageCount());

    for(size_t i = 0; i < imageCount(); i++) {
        array<VkImageView, 3> attachments = {_msaaImageViews[i].get(), _depthImageViews[i].get(), _swapchainImageViews[i].get()};

        const VkExtent2D swapchainExtent = _extent;
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = _renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;

        const VkResult createResult = vkCreateFramebuffer(_device->get(), &framebufferInfo, nullptr, &_swapchainFramebuffers[i]);

        CTH_STABLE_ERR(createResult != VK_SUCCESS, "Vk: failed to create framebuffer")
            throw cth::except::vk_result_exception{createResult, details->exception()};
    }
}

void Swapchain::createSyncObjects() {
    _imageAvailableSemaphores.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    _inFlightFences.resize(Constants::MAX_FRAMES_IN_FLIGHT);
    _imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
        const VkResult createWaitSemaphoreResult = vkCreateSemaphore(_device->get(), &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]);
        const VkResult createSignalSemaphoreResult = vkCreateSemaphore(_device->get(), &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]);
        const VkResult createFenceResult = vkCreateFence(_device->get(), &fenceInfo, nullptr, &_inFlightFences[i]);

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


void Swapchain::init(const Surface* surface, DeletionQueue* deletion_queue) {
    _msaaSamples = evalMsaaSampleCount();

    createSwapchain(surface);

    createSwapchainImages();
    createMsaaResources(deletion_queue);
    createDepthResources(deletion_queue);

    createRenderPass();

    createFramebuffers();
    createSyncObjects();
}



} // namespace cth
