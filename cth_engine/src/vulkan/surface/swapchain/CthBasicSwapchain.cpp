#include "CthBasicSwapchain.hpp"

#include "../graphics_core/CthGraphicsSyncConfig.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/control/CthPipelineBarrier.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/render/pass/CthAttachmentCollection.hpp"
#include "vulkan/render/pass/CthRenderPass.hpp"
#include "vulkan/render/pass/CthSubpass.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/resource/image/Framebuffer.hpp"
#include "vulkan/surface/CthSurface.hpp"

#include <vulkan/utility/cth_vk_exceptions.hpp>
#include <vulkan/utility/cth_vk_overloads.hpp>


namespace cth::vk {

BasicSwapchain::BasicSwapchain(cth::not_null<Core const*> core, cth::not_null<Queue const*> present_queue,
    cth::not_null<GraphicsSyncConfig const*> sync_config, cth::not_null<Surface const*> surface) :
    _core(core), _presentQueue(present_queue), _surface{surface}, _syncConfig(sync_config) {
    Core::debug_check(core.get());
    DEBUG_CHECK_SURFACE(surface);
    Queue::debug_check_present(present_queue);
    GraphicsSyncConfig::debug_check(sync_config);
    createSyncObjects();
    _imageIndices.fill(NO_IMAGE_INDEX);
}
BasicSwapchain::~BasicSwapchain() {
    DEBUG_CHECK_SWAPCHAIN_LEAK(this);
    destroySyncObjects(_core->destructionQueue());
}

void BasicSwapchain::create(VkExtent2D window_extent, VkSwapchainKHR old_swapchain) {
    //create the reset function
    _msaaSamples = evalMsaaSampleCount();

    createSwapchain(window_extent, old_swapchain);

    createRenderPass();

    createFramebuffers();

    createPresentInfos();
}

void BasicSwapchain::destroy() {

    destroyResources();

    destroySwapchain();

    reset();
}


void BasicSwapchain::resize(VkExtent2D window_extent) {

    VkSwapchainKHR old = _handle.release();

    destroyResources();
    resizeReset();
    create(window_extent, old);

    destroy(_core->vkDevice(), old);
}



VkResult BasicSwapchain::acquireNextImage(Cycle const& cycle) {
    auto const& fence = _imageAvailableFences[cycle.subIndex];
    auto const semaphore = _syncConfig->imageAvailableSemaphore(cycle.subIndex)->get();

    fence.wait();
    fence.reset();

    VkResult const acquireResult = vkAcquireNextImageKHR(_core->vkDevice(), _handle.get(), std::numeric_limits<uint64_t>::max(), semaphore,
        fence.get(), &_imageIndices[cycle.subIndex]);

    CTH_STABLE_ERR(acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR, "failed to acquire vk_image")
        throw cth::vk::result_exception{acquireResult, details->exception()};

    return acquireResult;
}
void BasicSwapchain::skipAcquire(Cycle const& cycle) const {
    auto const semaphore = _syncConfig->imageAvailableSemaphore(cycle.subIndex)->get();
    auto const& fence = _imageAvailableFences[cycle.subIndex];

    fence.wait();
    fence.reset();


    auto const submitInfo = VkSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 0,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &semaphore,
    };

    auto const result = vkQueueSubmit(_presentQueue->get(), 1, &submitInfo, fence.get());

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to skip-acquire an vk_image")
        throw cth::vk::result_exception{result, details->exception()};
}
void BasicSwapchain::beginRenderPass(Cycle const& cycle, PrimaryCmdBuffer const* cmd_buffer) const {
    _renderPass->begin(cmd_buffer, 0, &_swapchainFramebuffers[_imageIndices[cycle.subIndex]]);

    VkViewport const viewport{
        .x = 0,
        .y = 0,
        .width = static_cast<float>(_extent.width),
        .height = static_cast<float>(_extent.height),
        .minDepth = 0,
        .maxDepth = 1.0f,
    };
    VkRect2D const scissor{
        .offset = {0, 0},
        .extent = _extent
    };
    vkCmdSetViewport(cmd_buffer->get(), 0, 1, &viewport);
    vkCmdSetScissor(cmd_buffer->get(), 0, 1, &scissor);
}
void BasicSwapchain::endRenderPass(PrimaryCmdBuffer const* cmd_buffer) { _renderPass->end(cmd_buffer); }

VkResult BasicSwapchain::present(Cycle const& cycle) {
    size_t subIndex = cycle.subIndex;

    CTH_ERR(_imageIndices[subIndex] == NO_IMAGE_INDEX, "no acquired vk_image available") {
        details->add("frame: ({})", subIndex);
        throw details->exception();
    }

    auto const result = _presentQueue->present(_imageIndices[subIndex], _presentInfos[subIndex]);

    _imageIndices[subIndex] = NO_IMAGE_INDEX;

    return result;
}
void BasicSwapchain::skipPresent(Cycle const& cycle) {
    auto const& subIndex = cycle.subIndex;

    auto& imageIndex = _imageIndices[subIndex];

    CTH_WARN(imageIndex != NO_IMAGE_INDEX, "skip presenting an acquired vk_image, it will be discarded") {}

    _presentQueue->const_skip(_presentInfos[subIndex]);

    imageIndex = NO_IMAGE_INDEX;
}

void BasicSwapchain::changeSwapchainImageQueue(uint32_t release_queue, CmdBuffer const& release_cmd_buffer, uint32_t acquire_queue,
    CmdBuffer const& acquire_cmd_buffer, uint32_t image_index) {
    //TEMP test this function
    std::unordered_map<Image*, ImageBarrier::Info> const images{
        {_resolveAttachments->image(image_index), ImageBarrier::Info::QueueTransition(0, release_queue, 0, acquire_queue)}
    };

    ImageBarrier releaseBarrier{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, images};
    releaseBarrier.execute(release_cmd_buffer);

    ImageBarrier barrier{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, images};
    barrier.execute(acquire_cmd_buffer);
}

ImageView const* BasicSwapchain::imageView(size_t index) const { return _resolveAttachments->view(index); }
Image const* BasicSwapchain::image(size_t index) const { return _resolveAttachments->image(index); }


void BasicSwapchain::destroy(VkDevice device, VkSwapchainKHR swapchain) {
    DEBUG_CHECK_DEVICE_HANDLE(device);
    CTH_WARN(swapchain == VK_NULL_HANDLE, "swapchain should not be invalid (VK_NULL_HANDLE)") {}

    vkDestroySwapchainKHR(device, swapchain, nullptr);
}



VkSampleCountFlagBits BasicSwapchain::evalMsaaSampleCount() const {
    uint32_t const maxSamples = _core->physicalDevice()->maxSampleCount() / 2; //TODO add proper max_sample_count selection

    uint32_t samples = 1;
    while(samples < maxSamples && samples < constants::MAX_MSAA_SAMPLES) samples *= 2;

    return static_cast<VkSampleCountFlagBits>(samples);
}

void BasicSwapchain::createSyncObjects() {
    _imageAvailableFences.reserve(constants::FRAMES_IN_FLIGHT);
    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++)
        _imageAvailableFences.emplace_back(_core, VK_FENCE_CREATE_SIGNALED_BIT);
}

VkSurfaceFormatKHR BasicSwapchain::chooseSwapSurfaceFormat(std::span<VkSurfaceFormatKHR const> available_formats,
    std::span<VkSurfaceFormatKHR const> allowed_formats) {
    for(auto const& format : allowed_formats)
        if(std::ranges::contains(available_formats, format)) return format;


    CTH_STABLE_WARN(true, "no suitable format found, choosing first available") {
        details->add("available: {}", available_formats);
        details->add("allowed: {}", allowed_formats);
        details->add("chosen: {}", available_formats[0]);
        return available_formats[0];
    }
}
VkPresentModeKHR BasicSwapchain::chooseSwapPresentMode(std::span<VkPresentModeKHR const> available_present_modes,
    std::span<VkPresentModeKHR const> allowed_present_modes) {
    for(auto const mode : allowed_present_modes)
        if(std::ranges::contains(available_present_modes, mode)) {
            cth::log::msg<except::INFO>("present mode: {}", mode);
            return mode;
        }

    CTH_STABLE_WARN(true, "none of the allowed present modes were available, falling back to FIFO") {
        details->add("available: {}", available_present_modes);
        details->add("allowed: {}", allowed_present_modes);
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}
VkExtent2D BasicSwapchain::chooseSwapExtent(VkExtent2D window_extent, VkSurfaceCapabilitiesKHR const& capabilities) {
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;

    VkExtent2D const extent{
        .width = std::max(capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, window_extent.width)),
        .height = std::max(capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, window_extent.height))
    };

    return extent;
}
uint32_t BasicSwapchain::evalMinImageCount(uint32_t min, uint32_t max) {
    uint32_t imageCount = min + 1; //TODO check if this is really wrong if the imageCount is 4
    if(max > 0 && imageCount > max) imageCount = max;


    log::msg<except::INFO>("vk_image count: {0}, frames in flight: {1}", min + 1, constants::FRAMES_IN_FLIGHT);

    return imageCount;
}

VkSwapchainCreateInfoKHR BasicSwapchain::createInfo(VkSurfaceKHR surface,
    VkSurfaceFormatKHR surface_format, VkSurfaceCapabilitiesKHR const& capabilities, VkPresentModeKHR present_mode, VkExtent2D extent,
    uint32_t image_count, VkSwapchainKHR old_swapchain) {

    VkSwapchainCreateInfoKHR const createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,

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

        .oldSwapchain = old_swapchain,
    };
    return createInfo;
}

void BasicSwapchain::createSwapchain(VkExtent2D window_extent, VkSwapchainKHR old_swapchain) {
    DEBUG_CHECK_SWAPCHAIN_LEAK(this);
    DEBUG_CHECK_SWAPCHAIN_WINDOW_EXTENT(window_extent);

    _windowExtent = window_extent;

    auto const surfaceFormats = _surface->formats(*_core->physicalDevice());
    auto const presentModes = _surface->presentModes(*_core->physicalDevice());
    auto const capabilities = _surface->capabilities(*_core->physicalDevice());

    auto const allowedSurfaceFormats = std::vector<VkSurfaceFormatKHR>{{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    std::vector<VkPresentModeKHR> const allowedPresentModes{VK_PRESENT_MODE_FIFO_KHR};
    //TEMP change to always FIFO {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR};

    VkSurfaceFormatKHR const surfaceFormat = chooseSwapSurfaceFormat(surfaceFormats, allowedSurfaceFormats);
    VkPresentModeKHR const presentMode = chooseSwapPresentMode(presentModes, allowedPresentModes);
    VkExtent2D const extent = chooseSwapExtent(window_extent, capabilities);

    uint32_t const imageCount = evalMinImageCount(capabilities.minImageCount, capabilities.maxImageCount);

    auto const info = createInfo(_surface->get(), surfaceFormat, capabilities, presentMode, extent, imageCount, old_swapchain);


    VkSwapchainKHR ptr = nullptr;
    VkResult const createResult = vkCreateSwapchainKHR(_core->vkDevice(), &info, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create swapchain")
        throw cth::vk::result_exception{createResult, details->exception()};

    _handle = ptr;

    _imageFormat = surfaceFormat.format;
    _extent = extent;
    _aspectRatio = static_cast<float>(_extent.width) / static_cast<float>(_extent.height);
}



Image::Config BasicSwapchain::createColorImageConfig(VkSampleCountFlagBits samples) const {
    CTH_ERR(_imageFormat == VK_FORMAT_UNDEFINED, "image format must not be VK_FORMAT_UNDEFINED")
        throw details->exception();

    return Image::Config{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = _imageFormat,
        .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .samples = samples,
    };
}
Image::Config BasicSwapchain::createDepthImageConfig() const {
    CTH_ERR(_depthFormat == VK_FORMAT_UNDEFINED, "depth format must not be VK_FORMAT_UNDEFINED")
        throw details->exception();

    return Image::Config{
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .format = _depthFormat,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .samples = _msaaSamples,
    };
}
std::vector<std::unique_ptr<Image>> BasicSwapchain::getSwapchainImages() {
    uint32_t imageCount; //only min specified, might be higher
    auto const countResult = vkGetSwapchainImagesKHR(_core->vkDevice(), _handle.get(), &imageCount, nullptr);

    CTH_STABLE_ERR(countResult != VK_SUCCESS, "failed to get swapchain image count")
        throw cth::vk::result_exception{countResult, details->exception()};

    _imageCount = imageCount;

    std::vector<VkImage> vkImages{imageCount};
    auto const getResult = vkGetSwapchainImagesKHR(_core->vkDevice(), _handle.get(), &imageCount, vkImages.data());

    CTH_STABLE_ERR(getResult != VK_SUCCESS, "failed to get swapchain images")
        throw cth::vk::result_exception{getResult, details->exception()};


    std::vector<std::unique_ptr<Image>> images{};
    images.reserve(imageCount);

    auto const imageConfig = createColorImageConfig(VK_SAMPLE_COUNT_1_BIT);
    for(auto const& vkImage : vkImages)
        images.emplace_back(std::make_unique<Image>(_core, imageConfig, Image::State{_extent, vkImage, true, nullptr}));

    return images;
}


void BasicSwapchain::createMsaaAttachments() {
    AttachmentDescription description{
        .samples = _msaaSamples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .referenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    auto const imageConfig = createColorImageConfig(_msaaSamples);

    _msaaAttachments = std::make_unique<AttachmentCollection>(_core, imageCount(), 0, imageConfig, description, _extent);
}
void BasicSwapchain::findDepthFormat() {
    _depthFormat = _core->physicalDevice()->findSupportedFormat(
        std::vector{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
void BasicSwapchain::createDepthAttachments() {
    AttachmentDescription description{
        .samples = _msaaSamples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .referenceLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    auto const imageConfig = createDepthImageConfig();

    _depthAttachments = std::make_unique<AttachmentCollection>(_core, imageCount(), 1, imageConfig, description, _extent);
}

void BasicSwapchain::createResolveAttachments(std::vector<std::unique_ptr<Image>> swapchain_images) {
    AttachmentDescription description{
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .referenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    AttachmentCollection::State state{_extent};

    for(auto& image : swapchain_images) state.images.emplace_back(std::move(image));


    _resolveAttachments = std::make_unique<AttachmentCollection>(_core, imageCount(), 2, state.images[0]->config(), description, std::move(state));
}

void BasicSwapchain::createAttachments() {
    findDepthFormat();
    auto swapchainImages = getSwapchainImages();

    createMsaaAttachments();
    createDepthAttachments();
    createResolveAttachments(std::move(swapchainImages));

}



void BasicSwapchain::createSubpass() {
    _subpass = std::make_unique<Subpass>(
        0u,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        std::vector<AttachmentCollection*>{},
        std::vector{_msaaAttachments.get()},
        std::vector{_resolveAttachments.get()},
        _depthAttachments.get(),
        std::vector<AttachmentCollection*>{}
        );
}
VkSubpassDependency BasicSwapchain::createSubpassDependency() const {
    return VkSubpassDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };
}


void BasicSwapchain::createRenderPass() {
    createAttachments();
    createSubpass();

    auto const subpassDependency = createSubpassDependency();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0, 0, 0, 1}}; // NOLINT(cppcoreguidelines-pro-type-union-access)
    clearValues[1].depthStencil = {1.0f, 0}; // NOLINT(cppcoreguidelines-pro-type-union-access)

    RenderPass::BeginConfig const beginConfig{
        .clearValues = clearValues,
        .extent = _extent,
    };

    _renderPass = std::make_unique<RenderPass>(_core, std::vector{_subpass.get()}, std::vector{subpassDependency}, std::vector{beginConfig}, true);
}


void BasicSwapchain::createFramebuffers() {
    _swapchainFramebuffers.reserve(imageCount());

    for(size_t i = 0; i < imageCount(); i++) {
        std::array attachments = {_msaaAttachments->view(i), _depthAttachments->view(i), _resolveAttachments->view(i)};

        _swapchainFramebuffers.emplace_back(_core, _renderPass.get(), attachments, _extent);
    }
}



void BasicSwapchain::createPresentInfos() {
    _presentInfos.reserve(constants::FRAMES_IN_FLIGHT);

    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++) {
        std::vector<Semaphore const*> semaphores{_syncConfig->renderFinishedSemaphore(i)};
        _presentInfos.emplace_back(this, semaphores);
    }
}


void BasicSwapchain::destroyRenderConstructs() {
    _presentInfos.clear();
    _swapchainFramebuffers.clear();
    _renderPass = nullptr;
    _subpass = nullptr;
}

void BasicSwapchain::destroyResources() {
    destroyRenderConstructs();


    auto resolveAttachmentState = _resolveAttachments->release();
    for(auto const& image : resolveAttachmentState.images)
        [[maybe_unused]] auto const result = image->release();
    resolveAttachmentState.views.clear();


    _resolveAttachments = nullptr;
    _depthAttachments = nullptr;
    _msaaAttachments = nullptr;
}

void BasicSwapchain::destroySwapchain() {
    auto const lambda = [device = _core->vkDevice(), swapchain = _handle.get()]() { destroy(device, swapchain); };

    auto const& queue = _core->destructionQueue();

    if(queue) queue->push(lambda);
    else lambda();

    reset();
}

void BasicSwapchain::destroySyncObjects(DestructionQueue* destruction_queue) {
    for(auto& fence : _imageAvailableFences) fence.destroy();
    _imageAvailableFences.clear();
}
void BasicSwapchain::resizeReset() {
    _extent = {};
    _windowExtent = {};
    _aspectRatio = 0;
    _depthFormat = VK_FORMAT_UNDEFINED;
    _imageFormat = VK_FORMAT_UNDEFINED;
    _imageCount = 0;
    _imageIndices.fill(NO_IMAGE_INDEX);
}

void BasicSwapchain::reset() {
    _handle = VK_NULL_HANDLE;
    resizeReset();
}



#ifdef CONSTANT_DEBUG_MODE
void BasicSwapchain::debug_check(BasicSwapchain const* swapchain) {
    CTH_ERR(swapchain == nullptr, "swapchain invalid (nullptr)") throw details->exception();
    CTH_ERR(swapchain->_handle == VK_NULL_HANDLE, "swapchain handle invalid (VK_NULL_HANDLE)") throw details->exception();
}
void BasicSwapchain::debug_check_leak(BasicSwapchain const* swapchain) {
    CTH_WARN(swapchain->_handle != VK_NULL_HANDLE, "swapchain handle replaced, (potential memory leak)") {}
}
void BasicSwapchain::debug_check_window_extent(VkExtent2D window_extent) {
    CTH_ERR(window_extent.width == 0 || window_extent.height == 0, "window_extent width({0}) or height({0}) invalid (> 0 required",
        window_extent.width, window_extent.height) throw details->exception();
}
void BasicSwapchain::debug_check_compatibility(BasicSwapchain const& a, BasicSwapchain const& b) {
    CTH_ERR(a._core == b._core, "swapchains not compatible (different cores)") throw details->exception();
}
#endif



} // namespace cth

//TEMP old code

//BasicSwapchain::BasicSwapchain(const Core* core, DestructionQueue* destruction_queue, const Surface* surface, const Queue* present_queue,
//    const VkExtent2D window_extent) : _core(core), _presentQueue(present_queue), _windowExtent(window_extent), { init(surface, destruction_queue); }
//BasicSwapchain::BasicSwapchain(const Core* core, DestructionQueue* destruction_queue, const Surface* surface, const Queue* present_queue,
//    const VkExtent2D window_extent, shared_ptr<BasicSwapchain> previous) : _core{core}, _presentQueue(present_queue), _windowExtent(window_extent),
//    _oldSwapchain{std::move(previous)} {
//    init(surface, destruction_queue);
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
//    submitInfo.pWaitSemaphores = &_imageAvailableSemaphores[_frameIndex];
//
//    constexpr array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
//    submitInfo.pWaitDstStageMask = waitStages.data();
//    submitInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());
//    submitInfo.pCommandBuffers = cmdBuffers.data();
//
//    submitInfo.signalSemaphoreCount = 1u;
//    submitInfo.pSignalSemaphores = &_renderFinishedSemaphores[_frameIndex];
//
//    return vkQueueSubmit(_presentQueue->get(), 1, &submitInfo, _inFlightFences[_frameIndex]);
//}

//submitCommandBuffer()
//if(_imagesInFlight[image_index].get() != VK_NULL_HANDLE) {
//    vkWaitForFences(_core->vkDevice(), 1, &_imagesInFlight[image_index], VK_TRUE, UINT64_MAX);
//    destruction_queue->clear(static_cast<uint32_t>(_frameIndex));
//destruction_queue->create(static_cast<uint32_t>(_frameIndex)); 
//
//_imagesInFlight[image_index] = _inFlightFences[_frameIndex];
//
//vkResetFences(_core->vkDevice(), 1, &_inFlightFences[_frameIndex]);
//
//
//const VkResult submitResult = submit(vector{cmd_buffer});
//CTH_STABLE_ERR(submitResult != VK_SUCCESS, "failed to submit draw call")
//throw cth::vk::result_exception{submitResult, details->exception()};
//
//const auto presentResult = present(image_index);
//
//++_frameIndex %= constants::FRAMES_IN_FLIGHT;
