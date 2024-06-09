#include "CthBasicSwapchain.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthPipelineBarrier.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/resource/image/CthImage.hpp"
#include "vulkan/surface/CthSurface.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <array>
#include <limits>



namespace cth {

//BasicSwapchain::BasicSwapchain(const BasicCore* core, DeletionQueue* deletion_queue, const Surface* surface, const Queue* present_queue,
//    const VkExtent2D window_extent) : _core(core), _presentQueue(present_queue), _windowExtent(window_extent), { init(surface, deletion_queue); }
//BasicSwapchain::BasicSwapchain(const BasicCore* core, DeletionQueue* deletion_queue, const Surface* surface, const Queue* present_queue,
//    const VkExtent2D window_extent, shared_ptr<BasicSwapchain> previous) : _core{core}, _presentQueue(present_queue), _windowExtent(window_extent),
//    _oldSwapchain{std::move(previous)} {
//    init(surface, deletion_queue);
//    _oldSwapchain = nullptr;
//}


//BasicSwapchain::~BasicSwapchain() {
//
//    if(_vkSwapchain != nullptr) {
//        vkDestroySwapchainKHR(_core->vkDevice(), _vkSwapchain, nullptr);
//        _vkSwapchain = nullptr;
//    }
//
//    _swapchainImages.clear();
//    _msaaImages.clear();
//    _msaaImageViews.clear();
//    _depthImages.clear();
//    _depthImageViews.clear();
//
//
//    ranges::for_each(_swapchainFramebuffers, [this](VkFramebuffer framebuffer) { vkDestroyFramebuffer(_core->vkDevice(), framebuffer, nullptr); });
//
//    vkDestroyRenderPass(_core->vkDevice(), _renderPass, nullptr);
//
//    for(size_t i = 0; i < Constant::MAX_FRAMES_IN_FLIGHT; i++) {
//        vkDestroySemaphore(_core->vkDevice(), _renderFinishedSemaphores[i], nullptr);
//        vkDestroySemaphore(_core->vkDevice(), _imageAvailableSemaphores[i], nullptr);
//        vkDestroyFence(_core->vkDevice(), _inFlightFences[i], nullptr);
//    }
//}
BasicSwapchain::BasicSwapchain(const BasicCore* core, const Queue* present_queue) : _core(core), _presentQueue(present_queue) {
    DEBUG_CHECK_CORE(core);
}
BasicSwapchain::~BasicSwapchain() {
    //TEMP left off here implement a reset function, maybe a render pass abstraction
    //TEMP implement the non basic variant of swapchain
}

void BasicSwapchain::create(const Surface* surface, const VkExtent2D window_extent, const BasicSwapchain* old_swapchain) {

    DEBUG_CHECK_SURFACE(surface);
    DEBUG_CHECK_SWAPCHAIN_LEAK(this);
    DEBUG_CHECK_SWAPCHAIN_WINDOW_EXTENT(window_extent);
    DEBUG_CHECK_SWAPCHAIN_NULLPTR_ALLOWED(old_swapchain);

    _windowExtent = window_extent;
    _surface = surface;

    const auto surfaceFormats = surface->formats(*_core->physicalDevice());
    const auto presentModes = surface->presentModes(*_core->physicalDevice());
    const auto capabilities = surface->capabilities(*_core->physicalDevice());

    const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(surfaceFormats);
    const VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
    const VkExtent2D extent = chooseSwapExtent(window_extent, capabilities);

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

    createInfo.oldSwapchain = old_swapchain ? old_swapchain->get() : VK_NULL_HANDLE;

    const VkResult createResult = vkCreateSwapchainKHR(_core->vkDevice(), &createInfo, nullptr, &_vkSwapchain);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create swapchain")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    _imageFormat = surfaceFormat.format;
    _extent = extent;
}
void BasicSwapchain::destroy(DeletionQueue* deletion_queue) {



    if(deletion_queue) deletion_queue->push(_vkSwapchain);
    else destroy(_core->vkDevice(), _vkSwapchain);

    _vkSwapchain = VK_NULL_HANDLE;
}


void BasicSwapchain::resize(const VkExtent2D window_extent) { relocate(_surface, window_extent); }
void BasicSwapchain::relocate(const Surface* surface, const VkExtent2D window_extent) {
    VkSwapchainKHR old = _vkSwapchain;

    create(surface, window_extent, this);

    destroy(_core->vkDevice(), old);
}



VkResult BasicSwapchain::acquireNextImage(uint32_t* image_index) const {
    vkWaitForFences(_core->vkDevice(), 1, &_inFlightFences[_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

    const VkResult result = vkAcquireNextImageKHR(_core->vkDevice(), _vkSwapchain, std::numeric_limits<uint64_t>::max(),
        _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, image_index);
    
    return result;
}
//TEMP remove the deletion queue from here and this entire function into the renderer
VkResult BasicSwapchain::submitCommandBuffer(DeletionQueue* deletion_queue, const PrimaryCmdBuffer* cmd_buffer, const uint32_t image_index) {


    if(_imagesInFlight[image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(_core->vkDevice(), 1, &_imagesInFlight[image_index], VK_TRUE, UINT64_MAX);
        deletion_queue->clear(static_cast<uint32_t>(_currentFrame)); //TEMP remove this from here
    }
    deletion_queue->next(static_cast<uint32_t>(_currentFrame)); //TEMP remove this from here

    _imagesInFlight[image_index] = _inFlightFences[_currentFrame];

    vkResetFences(_core->vkDevice(), 1, &_inFlightFences[_currentFrame]);


    const VkResult submitResult = submit(vector{cmd_buffer});
    CTH_STABLE_ERR(submitResult != VK_SUCCESS, "failed to submit draw call")
        throw cth::except::vk_result_exception{submitResult, details->exception()};

    const auto presentResult = present(image_index);

    ++_currentFrame %= Constant::FRAMES_IN_FLIGHT;
    return presentResult;
}
void BasicSwapchain::changeSwapchainImageQueue(const uint32_t release_queue, const CmdBuffer& release_cmd_buffer, const uint32_t acquire_queue,
    const CmdBuffer& acquire_cmd_buffer, const uint32_t image_index) {
    const std::unordered_map<BasicImage*, ImageBarrier::Info> images{
        {&_swapchainImages[image_index], ImageBarrier::Info::QueueTransition(0, release_queue, 0, acquire_queue)}
    };

    ImageBarrier releaseBarrier{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, images};
    releaseBarrier.execute(release_cmd_buffer);

    ImageBarrier barrier{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, images};
    barrier.execute(acquire_cmd_buffer);
}


void BasicSwapchain::destroy(VkDevice device, VkSwapchainKHR swapchain) {
    DEBUG_CHECK_DEVICE_HANDLE(device);
    CTH_WARN(swapchain == VK_NULL_HANDLE, "swapchain should not be invalid (VK_NULL_HANDLE)");

    vkDestroySwapchainKHR(device, swapchain, nullptr);
}


VkResult BasicSwapchain::submit(vector<const PrimaryCmdBuffer*> cmd_buffers) const {
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

    return vkQueueSubmit(_core->graphicsQueue(), 1, &submitInfo, _inFlightFences[_currentFrame]);
}
VkResult BasicSwapchain::present(const uint32_t image_index) const {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &_renderFinishedSemaphores[_currentFrame];

    const array<VkSwapchainKHR, 1> swapchains{_vkSwapchain};

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains.data();
    presentInfo.pImageIndices = &image_index;


    return vkQueuePresentKHR(_core->presentQueue(), &presentInfo);
}

VkSampleCountFlagBits BasicSwapchain::evalMsaaSampleCount() const {
    const uint32_t maxSamples = _core->physicalDevice()->maxSampleCount() / 2; //TODO add proper max_sample_count selection

    uint32_t samples = 1;
    while(samples < maxSamples && samples < Constant::MAX_MSAA_SAMPLES) samples *= 2;

    return static_cast<VkSampleCountFlagBits>(samples);
}

VkSurfaceFormatKHR BasicSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) {
    const auto it = ranges::find_if(available_formats, [](const VkSurfaceFormatKHR& available_format) {
        return available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });

    CTH_STABLE_WARN(it == available_formats.end(), "no suitable format found, choosing [0]") return available_formats[0];

    return *it;
}
VkPresentModeKHR BasicSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) {
    const auto it = ranges::find(available_present_modes, VK_PRESENT_MODE_MAILBOX_KHR);

    /* if(it != available_present_modes.end()) {
        cth::log::msg<except::INFO>("present mode: MAILBOX");
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }*/

    //VK_PRESENT_MODE_IMMEDIATE_KHR


    cth::log::msg<except::INFO>("present mode: FIFO (V-Sync)");
    return VK_PRESENT_MODE_FIFO_KHR;

}
VkExtent2D BasicSwapchain::chooseSwapExtent(const VkExtent2D window_extent, const VkSurfaceCapabilitiesKHR& capabilities) {
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;

    VkExtent2D extent{
        .width = std::max(capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, window_extent.width)),
        .height = std::max(capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, window_extent.height))
    };

    return extent;
}
uint32_t BasicSwapchain::evalMinImageCount(const uint32_t min, const uint32_t max) {
    uint32_t imageCount = min + 1; //TODO check if this is really wrong if the imageCount is 4
    if(max > 0 && imageCount > max) imageCount = max;
    return imageCount;
}



BasicImage::Config BasicSwapchain::createImageConfig() const {
    BasicImage::Config config;
    config.samples = _msaaSamples;

    config.aspectMask = VK_IMAGE_ASPECT_NONE;
    config.format = VK_FORMAT_UNDEFINED;
    config.usage = VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;


    config.mipLevels = 1;
    config.tiling = VK_IMAGE_TILING_OPTIMAL;

    return config;
}
BasicImage::Config BasicSwapchain::createColorImageConfig() const {
    auto config = createImageConfig();
    config.format = _imageFormat;
    config.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    config.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    return config;
}
BasicImage::Config BasicSwapchain::createDepthImageConfig() const {
    auto config = createImageConfig();
    config.format = _depthFormat;
    config.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    config.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    return config;
}

void BasicSwapchain::createSwapchainImages() {
    uint32_t imageCount; //only min specified, might be higher
    vkGetSwapchainImagesKHR(_core->vkDevice(), _vkSwapchain, &imageCount, nullptr);

    vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(_core->vkDevice(), _vkSwapchain, &imageCount, images.data());

    const auto imageConfig = createColorImageConfig();

    for(auto image : images)
        _swapchainImages.emplace_back(make_unique<BasicImage>(_core, _extent, imageConfig, image,
            BasicImage::State::Default()));

    for(auto i = 0u; i < imageCount; i++) _swapchainImageViews.emplace_back(_core, _swapchainImages[i].get(), ImageView::Config::Default());
}
void BasicSwapchain::createMsaaResources(DeletionQueue* deletion_queue) {
    const auto imageConfig = createColorImageConfig();

    _msaaImages.reserve(imageCount());
    _msaaImageViews.reserve(imageCount());

    for(uint32_t i = 0; i < imageCount(); i++) {
        _msaaImages.emplace_back(make_unique<Image>(_core, deletion_queue, _extent, imageConfig, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        _msaaImageViews.emplace_back(_core, _msaaImages.back().get(), ImageView::Config::Default());
    }
}
VkFormat BasicSwapchain::findDepthFormat() const {
    return _core->physicalDevice()->findSupportedFormat(
        vector{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
void BasicSwapchain::createDepthResources(DeletionQueue* deletion_queue) {
    _depthFormat = findDepthFormat();

    const auto imageConfig = createDepthImageConfig();

    _depthImages.reserve(imageCount());
    _depthImageViews.reserve(imageCount());

    for(uint32_t i = 0; i < imageCount(); i++) {
        _depthImages.emplace_back(make_unique<Image>(_core, deletion_queue, _extent, imageConfig, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        _depthImageViews.emplace_back(_core, _depthImages.back().get(), ImageView::Config::Default());
    }
}



VkAttachmentDescription BasicSwapchain::createColorAttachmentDescription() const {
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
VkAttachmentDescription BasicSwapchain::createDepthAttachment() const {
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
VkAttachmentDescription BasicSwapchain::createColorAttachmentResolve() const {
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

SubpassDescription BasicSwapchain::createSubpassDescription() const {
    constexpr VkAttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    constexpr VkAttachmentReference depthAttachmentRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    constexpr VkAttachmentReference colorAttachmentResolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    return SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, vector{colorAttachmentRef}, depthAttachmentRef, vector{colorAttachmentResolveRef});
}
VkSubpassDependency BasicSwapchain::createSubpassDependency() const {
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


void BasicSwapchain::createRenderPass() {
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

    const VkResult createResult = vkCreateRenderPass(_core->vkDevice(), &renderPassInfo, nullptr, &_renderPass);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create render pass")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}


void BasicSwapchain::createFramebuffers() {
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

        const VkResult createResult = vkCreateFramebuffer(_core->vkDevice(), &framebufferInfo, nullptr, &_swapchainFramebuffers[i]);

        CTH_STABLE_ERR(createResult != VK_SUCCESS, "Vk: failed to create framebuffer")
            throw cth::except::vk_result_exception{createResult, details->exception()};
    }
}

void BasicSwapchain::createSyncObjects() {
    _imageAvailableSemaphores.resize(Constant::FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(Constant::FRAMES_IN_FLIGHT);
    _inFlightFences.resize(Constant::FRAMES_IN_FLIGHT);
    _imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(size_t i = 0; i < Constant::FRAMES_IN_FLIGHT; i++) {
        const VkResult createWaitSemaphoreResult = vkCreateSemaphore(_core->vkDevice(), &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]);
        const VkResult createSignalSemaphoreResult = vkCreateSemaphore(_core->vkDevice(), &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]);
        const VkResult createFenceResult = vkCreateFence(_core->vkDevice(), &fenceInfo, nullptr, &_inFlightFences[i]);

        CTH_STABLE_ERR(!(createWaitSemaphoreResult == createSignalSemaphoreResult == createFenceResult == VK_SUCCESS),
            "failed to create synchronization objects") {
            if(createWaitSemaphoreResult != VK_SUCCESS) details->add("wait semaphore creation failed");
            if(createSignalSemaphoreResult != VK_SUCCESS) details->add("signal semaphore creation failed");
            if(createFenceResult != VK_SUCCESS) details->add("fence creation failed");

            throw cth::except::vk_result_exception{static_cast<VkResult>(createWaitSemaphoreResult | createSignalSemaphoreResult | createFenceResult),
                details->exception()};
        }
    }
}


void BasicSwapchain::init(const Surface* surface, DeletionQueue* deletion_queue) {
    _msaaSamples = evalMsaaSampleCount();

    createSwapchain(surface);

    createSwapchainImages();
    createMsaaResources(deletion_queue);
    createDepthResources(deletion_queue);

    createRenderPass();

    createFramebuffers();
    createSyncObjects();
}

#ifdef CONSTANT_DEBUG_MODE
void BasicSwapchain::debug_check(const BasicSwapchain* swapchain) {
    CTH_ERR(swapchain == nullptr, "swapchain invalid (nullptr)") throw details->exception();
    CTH_ERR(swapchain->_vkSwapchain == VK_NULL_HANDLE, "swapchain handle invalid (VK_NULL_HANDLE)") throw details->exception();
}
void BasicSwapchain::debug_check_leak(const BasicSwapchain* swapchain) {
    CTH_WARN(swapchain->_vkSwapchain != VK_NULL_HANDLE, "swapchain handle replaced, (potential memory leak)");
}
void BasicSwapchain::debug_check_window_extent(const VkExtent2D window_extent) {
    CTH_ERR(window_extent.width == 0 || window_extent.height == 0, "window_extent width({0}) or height({0}) invalid (> 0 required",
        window_extent.width, window_extent.height) throw details->exception();
}
void BasicSwapchain::debug_check_compatibility(const BasicSwapchain& a, const BasicSwapchain& b) {
    CTH_ERR(a._core == b._core, "swapchains not compatible (different cores)") throw details->exception();
}
#endif



} // namespace cth
