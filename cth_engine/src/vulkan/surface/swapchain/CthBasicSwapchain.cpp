#include "CthBasicSwapchain.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthPipelineBarrier.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/resource/image/CthImage.hpp"
#include "vulkan/surface/CthSurface.hpp"
#include "vulkan/utility/CthVkUtils.hpp"



namespace cth::vk {

BasicSwapchain::BasicSwapchain(const BasicCore* core, const Queue* present_queue, BasicGraphicsSyncConfig sync_config) :
    _core(core), _presentQueue(present_queue), _syncConfig(std::move(sync_config)) {
    DEBUG_CHECK_CORE(core);
    createSyncObjects();
}
BasicSwapchain::~BasicSwapchain() {
    DEBUG_CHECK_SWAPCHAIN_LEAK(this);
    destroySyncObjects(nullptr);
}

void BasicSwapchain::create(const Surface* surface, const VkExtent2D window_extent, const BasicSwapchain* old_swapchain) {
    //TEMP continue here
    //temp 
    //create the reset function
    DEBUG_CHECK_SWAPCHAIN_LEAK(this);
    DEBUG_CHECK_SURFACE(surface);

    _msaaSamples = evalMsaaSampleCount();

    createSwapchain(surface, window_extent, old_swapchain);

    createSwapchainImages();
    createMsaaResources(nullptr);
    createDepthResources(nullptr);

    createRenderPass();

    createFramebuffers();
}

void BasicSwapchain::destroy(DeletionQueue* deletion_queue) {

    destroyResources(deletion_queue);

    destroySwapchain(deletion_queue);
}
void BasicSwapchain::destroyResources(DeletionQueue* deletion_queue) {
    clearPresentInfos();
    destroyFramebuffers(deletion_queue);
    destroyRenderPass(deletion_queue);
    destroyImages(deletion_queue);
}


void BasicSwapchain::resize(const VkExtent2D window_extent) { relocate(_surface, window_extent); }
void BasicSwapchain::relocate(const Surface* surface, const VkExtent2D window_extent) {
    VkSwapchainKHR old = _handle.get();
    destroyResources(); //TEMP deletion_queue?
    create(surface, window_extent, this);

    destroy(_core->vkDevice(), old); //TEMP deletion_queue?
}



VkResult BasicSwapchain::acquireNextImage() {
    if(_imageIndices[_currentFrame] == NO_IMAGE_INDEX)
        if(const auto result = acquireNewImage(_currentFrame); result != VK_SUCCESS) return result;


    return acquireNewImage(nextFrame(_currentFrame));
}
void BasicSwapchain::beginRenderPass(const PrimaryCmdBuffer* cmd_buffer) const {

    //TEMP move part of this to the framebuffer & render pass abstractions

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _renderPass;
    renderPassInfo.framebuffer = framebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = _extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0, 0, 0, 1}; // NOLINT(cppcoreguidelines-pro-type-union-access)
    clearValues[1].depthStencil = {1.0f, 0}; // NOLINT(cppcoreguidelines-pro-type-union-access)
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd_buffer->get(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(_extent.width);
    viewport.height = static_cast<float>(_extent.height);
    viewport.minDepth = 0;
    viewport.maxDepth = 1.0f;
    const VkRect2D scissor{{0, 0}, _extent};
    vkCmdSetViewport(cmd_buffer->get(), 0, 1, &viewport);
    vkCmdSetScissor(cmd_buffer->get(), 0, 1, &scissor);
}
void BasicSwapchain::endRenderPass(const PrimaryCmdBuffer* cmd_buffer) { vkCmdEndRenderPass(cmd_buffer->get()); }

VkResult BasicSwapchain::present(DeletionQueue* deletion_queue) {
    CTH_ERR(_imageIndices[_currentFrame] != NO_IMAGE_INDEX, "no acquired image available")
        throw details->exception();


    const auto result = _presentQueue->present(_imageIndices[_currentFrame], _presentInfos[_currentFrame]);

    const auto& fence = _imageAvailableFences[_currentFrame];


    // ReSharper disable once CppExpressionWithoutSideEffects
    fence.wait();
    deletion_queue->clear(static_cast<uint32_t>(_currentFrame)); //TEMP remove this from here

    deletion_queue->next(static_cast<uint32_t>(_currentFrame)); //TEMP remove this from here

    fence.reset();

    _imageIndices[_currentFrame] = NO_IMAGE_INDEX;


    _currentFrame = nextFrame(_currentFrame);

    return result;
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
    CTH_WARN(swapchain == VK_NULL_HANDLE, "swapchain should not be invalid (VK_NULL_HANDLE)") {}

    vkDestroySwapchainKHR(device, swapchain, nullptr);
}



VkResult BasicSwapchain::acquireNewImage(const size_t frame) {
    const VkResult acquireResult = vkAcquireNextImageKHR(_core->vkDevice(), _handle.get(), std::numeric_limits<uint64_t>::max(),
        _syncConfig.imageAvailableSemaphores[frame]->get(), _imageAvailableFences[frame].get(), &_imageIndices[frame]);

    CTH_STABLE_ERR(acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR, "failed to acquire image")
        throw cth::except::vk_result_exception{acquireResult, details->exception()};

    if(acquireResult == VK_SUBOPTIMAL_KHR) _imageIndices[frame] = NO_IMAGE_INDEX;


    return acquireResult;
}



VkSampleCountFlagBits BasicSwapchain::evalMsaaSampleCount() const {
    const uint32_t maxSamples = _core->physicalDevice()->maxSampleCount() / 2; //TODO add proper max_sample_count selection

    uint32_t samples = 1;
    while(samples < maxSamples && samples < constants::MAX_MSAA_SAMPLES) samples *= 2;

    return static_cast<VkSampleCountFlagBits>(samples);
}

void BasicSwapchain::createSyncObjects() {
    _imageAvailableFences.reserve(constants::FRAMES_IN_FLIGHT);
    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++) 
        _imageAvailableFences.emplace_back(_core, nullptr);
}

VkSurfaceFormatKHR BasicSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) {
    const auto it = std::ranges::find_if(available_formats, [](const VkSurfaceFormatKHR& available_format) {
        return available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });

    CTH_STABLE_WARN(it == available_formats.end(), "no suitable format found, choosing [0]") return available_formats[0];

    return *it;
}
VkPresentModeKHR BasicSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) {
    const auto it = std::ranges::find(available_present_modes, VK_PRESENT_MODE_MAILBOX_KHR);

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

    const VkExtent2D extent{
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


    log::msg<except::INFO>("image count: {0}, frames in flight: {1}", min + 1, constants::FRAMES_IN_FLIGHT);

    return imageCount;
}

VkSwapchainCreateInfoKHR BasicSwapchain::createInfo(const Surface* surface, const VkSurfaceFormatKHR surface_format,
    const VkSurfaceCapabilitiesKHR& capabilities, const VkPresentModeKHR present_mode, const VkExtent2D extent, const uint32_t image_count,
    const BasicSwapchain* old_swapchain) {

    const VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface->get(),

        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,


        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0, // Optional
        .pQueueFamilyIndices = nullptr, // Optional


        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

        .presentMode = present_mode,
        .clipped = VK_TRUE,

        .oldSwapchain = old_swapchain ? old_swapchain->get() : VK_NULL_HANDLE,
    };
    return createInfo;
}

void BasicSwapchain::createSwapchain(const Surface* surface, const VkExtent2D window_extent, const BasicSwapchain* old_swapchain) {
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

    const auto info = createInfo(surface, surfaceFormat, capabilities, presentMode, extent, imageCount, old_swapchain);


    VkSwapchainKHR ptr = nullptr;
    const VkResult createResult = vkCreateSwapchainKHR(_core->vkDevice(), &info, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create swapchain")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    _handle = ptr;

    _imageFormat = surfaceFormat.format;
    _extent = extent;
    _aspectRatio = static_cast<float>(_extent.width) / static_cast<float>(_extent.height);
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
    vkGetSwapchainImagesKHR(_core->vkDevice(), _handle.get(), &imageCount, nullptr);

    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(_core->vkDevice(), _handle.get(), &imageCount, images.data());

    const auto imageConfig = createColorImageConfig();

    for(auto image : images)
        _swapchainImages.emplace_back(_core, _extent, imageConfig, image,
            BasicImage::State::Default());

    for(auto i = 0u; i < imageCount; i++) _swapchainImageViews.emplace_back(_core, &_swapchainImages[i], ImageView::Config::Default());
}
void BasicSwapchain::createMsaaResources(DeletionQueue* deletion_queue) {
    const auto imageConfig = createColorImageConfig();

    _msaaImages.reserve(imageCount());
    _msaaImageViews.reserve(imageCount());

    for(uint32_t i = 0; i < imageCount(); i++) {
        _msaaImages.emplace_back(_core, deletion_queue, _extent, imageConfig, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        _msaaImageViews.emplace_back(_core, &_msaaImages.back(), ImageView::Config::Default());
    }
}
VkFormat BasicSwapchain::findDepthFormat() const {
    return _core->physicalDevice()->findSupportedFormat(
        std::vector{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
void BasicSwapchain::createDepthResources(DeletionQueue* deletion_queue) {
    _depthFormat = findDepthFormat();

    const auto imageConfig = createDepthImageConfig();

    _depthImages.reserve(imageCount());
    _depthImageViews.reserve(imageCount());

    for(uint32_t i = 0; i < imageCount(); i++) {
        _depthImages.emplace_back(_core, deletion_queue, _extent, imageConfig, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        _depthImageViews.emplace_back(_core, &_depthImages.back(), ImageView::Config::Default());
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

    return SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, std::vector{colorAttachmentRef}, depthAttachmentRef,
        std::vector{colorAttachmentResolveRef});
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

    const std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
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
        std::array<VkImageView, 3> attachments = {_msaaImageViews[i].get(), _depthImageViews[i].get(), _swapchainImageViews[i].get()};

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



void BasicSwapchain::createPresentInfos() {
    _presentInfos.reserve(constants::FRAMES_IN_FLIGHT);

    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++) {
        std::vector<const BasicSemaphore*> semaphores{_syncConfig.renderFinishedSemaphores[i]};
        _presentInfos.emplace_back(this, semaphores);
    }
}


void BasicSwapchain::clearPresentInfos() { _presentInfos.clear(); }

void BasicSwapchain::destroySyncObjects(DeletionQueue* deletion_queue) {
    for(auto& fence : _imageAvailableFences) fence.destroy(deletion_queue);
    _imageAvailableFences.clear();
}
void BasicSwapchain::destroyFramebuffers(DeletionQueue* deletion_queue) {
    for(const auto& framebuffer : _swapchainFramebuffers)
        vkDestroyFramebuffer(_core->vkDevice(), framebuffer, nullptr);
    _swapchainFramebuffers.clear();
}
void BasicSwapchain::destroyRenderPass(DeletionQueue* deletion_queue) {
    vkDestroyRenderPass(_core->vkDevice(), _renderPass, nullptr);
    _renderPass = VK_NULL_HANDLE;
}

void BasicSwapchain::destroyDepthImages(DeletionQueue* deletion_queue) {
    _depthImageViews.clear();

    for(auto& image : _depthImages) image.destroy(deletion_queue);
    _depthImages.clear();
}
void BasicSwapchain::destroyMsaaImages(DeletionQueue* deletion_queue) {
    _msaaImageViews.clear();

    for(auto& image : _msaaImages) image.destroy(deletion_queue);
    _msaaImages.clear();
}
void BasicSwapchain::destroySwapchainImages(DeletionQueue* deletion_queue) {
    _swapchainImageViews.clear();
    _swapchainImages.clear();
}
void BasicSwapchain::destroyImages(DeletionQueue* deletion_queue) {
    //TODO they dont have to be destroyed necessarily they can be kept sometimes
    destroyDepthImages(deletion_queue);
    destroyMsaaImages(deletion_queue);
    destroySwapchainImages(deletion_queue);
}
void BasicSwapchain::destroySwapchain(DeletionQueue* deletion_queue) {
    DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue)

    if(deletion_queue) deletion_queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    _handle = VK_NULL_HANDLE;
}



#ifdef CONSTANT_DEBUG_MODE
void BasicSwapchain::debug_check(const BasicSwapchain* swapchain) {
    CTH_ERR(swapchain == nullptr, "swapchain invalid (nullptr)") throw details->exception();
    CTH_ERR(swapchain->_handle == VK_NULL_HANDLE, "swapchain handle invalid (VK_NULL_HANDLE)") throw details->exception();
}
void BasicSwapchain::debug_check_leak(const BasicSwapchain* swapchain) {
    CTH_WARN(swapchain->_handle != VK_NULL_HANDLE, "swapchain handle replaced, (potential memory leak)") {}
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

//TEMP old code

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
//    for(size_t i = 0; i < constants::MAX_FRAMES_IN_FLIGHT; i++) {
//        vkDestroySemaphore(_core->vkDevice(), _renderFinishedSemaphores[i], nullptr);
//        vkDestroySemaphore(_core->vkDevice(), _imageAvailableSemaphores[i], nullptr);
//        vkDestroyFence(_core->vkDevice(), _inFlightFences[i], nullptr);
//    }
//}

//VkResult BasicSwapchain::submit(vector<const PrimaryCmdBuffer*> cmd_buffers) const {
//    static_assert(false, "this cannot be here");
//
//    vector<VkCommandBuffer> cmdBuffers(cmd_buffers.size());
//    ranges::transform(cmd_buffers, cmdBuffers.begin(), [](const PrimaryCmdBuffer* cmd_buffer) { return cmd_buffer->get(); });
//
//    VkSubmitInfo submitInfo = {};
//    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//
//    submitInfo.waitSemaphoreCount = 1u;
//    submitInfo.pWaitSemaphores = &_imageAvailableSemaphores[_currentFrame];
//
//    constexpr array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
//    submitInfo.pWaitDstStageMask = waitStages.data();
//    submitInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());
//    submitInfo.pCommandBuffers = cmdBuffers.data();
//
//    submitInfo.signalSemaphoreCount = 1u;
//    submitInfo.pSignalSemaphores = &_renderFinishedSemaphores[_currentFrame];
//
//    return vkQueueSubmit(_presentQueue->get(), 1, &submitInfo, _inFlightFences[_currentFrame]);
//}

//submitCommandBuffer()
//if(_imagesInFlight[image_index].get() != VK_NULL_HANDLE) {
//    vkWaitForFences(_core->vkDevice(), 1, &_imagesInFlight[image_index], VK_TRUE, UINT64_MAX);
//    deletion_queue->clear(static_cast<uint32_t>(_currentFrame));
//deletion_queue->next(static_cast<uint32_t>(_currentFrame)); 
//
//_imagesInFlight[image_index] = _inFlightFences[_currentFrame];
//
//vkResetFences(_core->vkDevice(), 1, &_inFlightFences[_currentFrame]);
//
//
//const VkResult submitResult = submit(vector{cmd_buffer});
//CTH_STABLE_ERR(submitResult != VK_SUCCESS, "failed to submit draw call")
//throw cth::except::vk_result_exception{submitResult, details->exception()};
//
//const auto presentResult = present(image_index);
//
//++_currentFrame %= constants::FRAMES_IN_FLIGHT;
