#include "jvk/surface/swapchain/swapchain.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/device.hpp"
#include "jvk/base/physical_device.hpp"
#include "jvk/base/queue/present_info.hpp"
#include "jvk/base/queue/submit_info.hpp"
#include "jvk/render/cmd/cmd_buffer.hpp"
#include "jvk/render/sync/fence.hpp"
#include "jvk/render/sync/pipeline_barrier.hpp"
#include "jvk/render/sync/pipeline_wait_stage.hpp"
#include "jvk/render/sync/semaphore.hpp"
#include "jvk/render/pass/attachment/attachment_collection.hpp"
#include "jvk/render/pass/framebuffer/framebuffer.hpp"
#include "jvk/res/destruction_queue.hpp"
#include "jvk/surface/surface.hpp"
#include "jvk/utility/vk_exceptions.hpp"


namespace jvk {

Swapchain::Swapchain(
    Core const& core,
    Queue const& present_queue,
    Surface const& surface,
    Config config
) : _core(&core),
    _presentQueue(&present_queue),
    _surface{&surface},
    _config{std::move(config)} { Config::debug_check(_config); }

Swapchain::Swapchain(
    Core const& core,
    Queue const& present_queue,
    Surface const& surface,
    Config const& config,
    VkExtent2D window_extent
) : Swapchain{core, present_queue, surface, config} { create(window_extent); }

Swapchain::~Swapchain() { optDestroy(); }

void Swapchain::create(VkExtent2D window_extent, VkSwapchainKHR old_swapchain) {
    Core::debug_check(*_core);
    Surface::debug_check(*_surface);
    Queue::debug_check_present(*_presentQueue);

    optDestroy();


    _imageIndices.fill(NO_IMAGE_INDEX);
    _msaaSamples = evalMsaaSampleCount();


    createSwapchain(window_extent, old_swapchain);

    createSyncObjects();

    createResolveAttachments();

    createPresentInfos(_config.renderFinishedSemaphores);
}

void Swapchain::destroy() {
    CTH_CRITICAL(!created(), "swapchain must be created") {}

    destroyResources();

    destroySwapchain(_handle.get());

    destroySyncObjects();

    reset();
}


void Swapchain::resize(VkExtent2D window_extent) {
    VkSwapchainKHR old = _handle.release();

    destroyResources();
    resizeReset();
    create(window_extent, old);


    destroySwapchain(old);
}



VkResult Swapchain::acquireNextImage(size_t in_flight_index) {
    auto& imageIndex = _imageIndices[in_flight_index];

    auto const& fence = _acquireFences[in_flight_index];
    fence.waitReset();

    auto const& semaphore = _config.imageAvailableSemaphores[in_flight_index];

    VkResult const acquireResult = _core->functions()->vkAcquireNextImageKHR(
        _core->vkDevice(),
        _handle.get(),
        std::numeric_limits<uint64_t>::max(),
        semaphore->get(),
        fence.get(),
        &imageIndex
    );

    JVK_RESULT_STABLE_THROW(
        acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR,
        acquireResult,
        "failed to acquire vk_image"
    );


    return acquireResult;
}

void Swapchain::skipAcquire(size_t in_flight_index) const {
    debug_check(*this);

    auto const& fence = _acquireFences[in_flight_index];
    fence.wait();

    auto const semaphore = _config.imageAvailableSemaphores[in_flight_index];
    syncSubmit(nullptr, semaphore);
}


VkResult Swapchain::present(size_t in_flight_index) {
    debug_check(*this);


    auto& imageIndex = _imageIndices[in_flight_index];

    CTH_CRITICAL(imageIndex == NO_IMAGE_INDEX, "no acquired vk_image available") {
        details->add("frame: ({})", in_flight_index);
    }

    auto const& presentSemaphore = _presentSemaphores[imageIndex];

    syncSubmit(
        _config.renderFinishedSemaphores[in_flight_index],
        &presentSemaphore
    );

    auto const result = vkPresent(presentSemaphore, imageIndex);
    imageIndex = NO_IMAGE_INDEX;

    return result;
}

void Swapchain::skipPresent(size_t in_flight_index) {
    debug_check(*this);

    auto& imageIndex = _imageIndices[in_flight_index];

    //TODO review this, change skipAcquire too or fix this implementation
    CTH_CRITICAL(imageIndex != NO_IMAGE_INDEX, "acquired images must be presented") {}

    syncSubmit(_config.renderFinishedSemaphores[in_flight_index], nullptr);

    imageIndex = NO_IMAGE_INDEX;
}

void Swapchain::changeSwapchainImageQueue(
    uint32_t release_queue,
    CmdBuffer const& release_cmd_buffer,
    uint32_t acquire_queue,
    CmdBuffer const& acquire_cmd_buffer,
    uint32_t image_index
) const {
    //TEMP test this function
    std::unordered_map<Image*, ImageBarrier::Info> const images{
        {
            _resolveAttachments->image(image_index),
            ImageBarrier::Info::QueueTransition(0, release_queue, 0, acquire_queue)
        }
    };

    ImageBarrier releaseBarrier{
        core(),
        {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT},
        images
    };
    releaseBarrier.execute(release_cmd_buffer);

    ImageBarrier barrier{
        core(),
        {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT},
        images
    };
    barrier.execute(acquire_cmd_buffer);
}

void Swapchain::addResolveSubpassDependencyFlags(VkSubpassDependency& swap_subpass_dependency) {
    swap_subpass_dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    swap_subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
}

void Swapchain::destroy(DeviceTable table, VkSwapchainKHR swapchain) {
    CTH_WARN(swapchain == VK_NULL_HANDLE, "swapchain should not be invalid (VK_NULL_HANDLE)") {}

    table->vkDestroySwapchainKHR(table.device(), swapchain, nullptr);
}



VkResult Swapchain::vkPresent(Semaphore const& semaphore, uint32_t image_index) const {
    auto vkPresentSemaphore = semaphore.get();
    auto swapchain = _handle.get();

    VkPresentInfoKHR const presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vkPresentSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &image_index,
        .pResults = nullptr
    };

    return _presentQueue->raw_present(presentInfo);
}


void Swapchain::syncSubmit(Semaphore const* wait, Semaphore const* signal) const {
    static constexpr VkPipelineStageFlags VK_WAIT_STAGE = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    auto vkWait = wait ? wait->get() : VK_NULL_HANDLE;
    auto vkSignal = signal ? signal->get() : VK_NULL_HANDLE;

    VkSubmitInfo const info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = vkWait != VK_NULL_HANDLE ? 1ul : 0ul,
        .pWaitSemaphores = &vkWait,
        .pWaitDstStageMask = &VK_WAIT_STAGE,
        .commandBufferCount = 0,
        .pCommandBuffers = nullptr,
        .signalSemaphoreCount = vkSignal != VK_NULL_HANDLE ? 1ul : 0ul,
        .pSignalSemaphores = &vkSignal
    };

    _presentQueue->raw_submit(info, nullptr);
}
void Swapchain::linkRenderFinishedSemaphores(size_t in_flight_index) const {
    auto const vkWaitSemaphore = _config.renderFinishedSemaphores[in_flight_index];
    auto const& vkSignalSemaphore = _presentSemaphores[_imageIndices[in_flight_index]];

    syncSubmit(vkWaitSemaphore, &vkSignalSemaphore);
}

void Swapchain::setImageFormat(VkFormat format) {
    _imageFormat = format;
    CTH_CRITICAL(_imageFormat == VK_FORMAT_UNDEFINED, "image format must not be VK_FORMAT_UNDEFINED") {}
}

VkSampleCountFlagBits Swapchain::evalMsaaSampleCount() const {
    uint32_t const maxSamples = _core->physicalDevice().maxSampleCount() / 2;
    //TODO add proper max_sample_count selection

    uint32_t samples = 1;
    while(samples < maxSamples && samples < constants::MAX_MSAA_SAMPLES)
        samples *= 2;

    return static_cast<VkSampleCountFlagBits>(samples);
}

VkExtent2D Swapchain::chooseSwapExtent(
    VkExtent2D window_extent,
    VkSurfaceCapabilitiesKHR const& capabilities
) {
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent; //TODO review this looks wrong

    VkExtent2D const extent{
        .width = std::max(
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, window_extent.width)
        ),
        .height = std::max(
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, window_extent.height)
        )
    };

    return extent;
}

uint32_t Swapchain::evalMinImageCount(uint32_t min, uint32_t max) {
    JVK_STABLE_THROW(
        min > max || min < 0 || max < constants::FRAMES_IN_FLIGHT,
        "invalid swapchain image count bounds"
    ) {
        details->add("requires 0 < min < max && min <= FRAMES_IN_FLIGHT <= max");
        details->add("min: {}, max: {}", min, max);
        details->add("FRAMES_IN_FLIGHT: {}", constants::FRAMES_IN_FLIGHT);
    }


    uint32_t imageCount = min + 1; //TODO check if this is really wrong if the imageCount is 4
    if(max > 0 && imageCount > max)
        imageCount = max;


    log::msg<except::INFO>(
        "vk_image count: {0}, frames in flight: {1}",
        min + 1,
        constants::FRAMES_IN_FLIGHT
    );

    return imageCount;
}

void Swapchain::refreshSize() {
    uint32_t imageCount; //only min specified, might be higher
    auto const countResult = _core->deviceTable()->vkGetSwapchainImagesKHR(
        _core->vkDevice(),
        _handle.get(),
        &imageCount,
        nullptr
    );

    CTH_STABLE_ERR(countResult != VK_SUCCESS, "failed to get swapchain image count")
    throw jvk::vk_result_exception{countResult, details->exception()};

    _imageCount = imageCount;
}
VkSwapchainCreateInfoKHR Swapchain::createInfo(
    VkSurfaceKHR surface,
    VkSurfaceFormatKHR surface_format,
    VkSurfaceCapabilitiesKHR const& capabilities,
    VkPresentModeKHR present_mode,
    VkExtent2D extent,
    uint32_t image_count,
    VkSwapchainKHR old_swapchain
) {
    VkSwapchainCreateInfoKHR const createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = surface,

        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,


        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        // Optional
        .pQueueFamilyIndices = nullptr,
        // Optional


        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

        .presentMode = present_mode,
        .clipped = VK_TRUE,

        .oldSwapchain = old_swapchain,
    };
    return createInfo;
}

void Swapchain::createSwapchain(VkExtent2D window_extent, VkSwapchainKHR old_swapchain) {
    Swapchain::debug_check_window_extent(window_extent);

    _windowExtent = window_extent;

    auto const capabilities = _surface->capabilities(_core->physicalDevice());

    auto const surfaceFormat = _surface->format(_core->physicalDevice());
    auto const presentMode = _surface->presentMode(_core->physicalDevice());

    auto const extent = chooseSwapExtent(window_extent, capabilities);

    uint32_t const imageCount = evalMinImageCount(capabilities.minImageCount, capabilities.maxImageCount);

    auto const info = createInfo(
        _surface->get(),
        surfaceFormat,
        capabilities,
        presentMode,
        extent,
        imageCount,
        old_swapchain
    );


    VkSwapchainKHR ptr = nullptr;
    auto const createResult = _core->deviceTable()->vkCreateSwapchainKHR(
        _core->vkDevice(),
        &info,
        nullptr,
        &ptr
    );

    JVK_RESULT_STABLE_THROW(createResult != VK_SUCCESS, createResult, "failed to create swapchain");

    _handle = ptr;

    setImageFormat(surfaceFormat.format);

    _extent = extent;
    _aspectRatio = static_cast<float>(_extent.width) / static_cast<float>(_extent.height);

    refreshSize();
}

void Swapchain::createSyncObjects() {
    for(size_t i = 0; i < _imageCount; i++)
        _presentSemaphores.emplace_back(*_core, jvk::create);

    _acquireFences.reserve(constants::FRAMES_IN_FLIGHT);
    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++)
        _acquireFences.emplace_back(*_core, VK_FENCE_CREATE_SIGNALED_BIT);
}



Image::Config Swapchain::createColorImageConfig(VkSampleCountFlagBits samples) const {
    //TEMP moved to frame resources in demo
    CTH_CRITICAL(_imageFormat == VK_FORMAT_UNDEFINED, "image format must not be VK_FORMAT_UNDEFINED") {}

    return Image::Config{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = _imageFormat,
        .usage = _config.subpassConfig.imageUsageFlags,

        //just filler memory is handled by driver
        .memoryProperties = std::nullopt,

        .samples = samples,
    };
}


auto Swapchain::getSwapchainImages() const -> std::vector<std::unique_ptr<Image>> {
    auto imageCount = static_cast<uint32_t>(_imageCount);

    std::vector<VkImage> vkImages{imageCount};
    auto const getResult = _core->deviceTable()->vkGetSwapchainImagesKHR(
        _core->vkDevice(),
        _handle.get(),
        &imageCount,
        vkImages.data()
    );

    JVK_RESULT_STABLE_THROW(getResult != VK_SUCCESS, getResult, "failed to get swapchain images");

    std::vector<std::unique_ptr<Image>> images{};
    images.reserve(imageCount);

    auto const imageConfig = createColorImageConfig(VK_SAMPLE_COUNT_1_BIT);
    for(auto const& vkImage : vkImages)
        images.emplace_back(
            std::make_unique<Image>(
                *_core,
                imageConfig,
                Image::State{_extent, vkImage, true, nullptr}
            )
        );

    return images;
}



void Swapchain::createResolveAttachments() {
    auto swapchainImages = getSwapchainImages();

    AttachmentDescription const description{
        .loadOp = _config.subpassConfig.imageAttachmentLoadOp,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .subpassLayouts = _config.subpassConfig.subpassLayouts
    };

    AttachmentCollection::State state{_extent};

    for(auto& image : swapchainImages)
        state.images.emplace_back(std::move(image));


    _resolveAttachments = std::make_unique<AttachmentCollection>(
        *_core,
        AttachmentCollection::Config{
            size(),
            constants::SWAPCHAIN_ATTACHMENT_INDEX,
            state.images[0]->config(),
            description
        },
        std::move(state)
    );
}



void Swapchain::createPresentInfos(std::span<Semaphore const* const> render_finished_semaphores) {
    cxpr auto frames = constants::FRAMES_IN_FLIGHT;

    _presentInfos.reserve(constants::FRAMES_IN_FLIGHT);

    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++) {
        std::vector semaphores{std::from_range, render_finished_semaphores | cth::views::drop_stride(i, frames)};

        _presentInfos.emplace_back(this, semaphores);
    }
}

void Swapchain::destroyResources() {
    _presentInfos.clear();


    auto resolveAttachmentState = _resolveAttachments->release();
    for(auto const& image : resolveAttachmentState.images)
        [[maybe_unused]] auto const result = image->release();
    resolveAttachmentState.views.clear();
}



void Swapchain::destroySwapchain(VkSwapchainKHR swapchain) const {
    auto const lambda = [table = _core->deviceTable(), swapchain]() { destroy(table, swapchain); };

    auto const& queue = _core->destructionQueue();

    if(queue)
        queue->push(lambda);
    else
        lambda();
}

void Swapchain::destroySyncObjects() {
    _acquireFences.clear();

    _presentQueue->wait();
    _presentSemaphores.clear();
}

void Swapchain::resizeReset() {
    _extent = {};
    _windowExtent = {};
    _aspectRatio = 0;
    _imageFormat = VK_FORMAT_UNDEFINED;
    _imageCount = 0;
    _imageIndices.fill(NO_IMAGE_INDEX);

    destroySyncObjects();
}

void Swapchain::reset() {
    _handle = VK_NULL_HANDLE;
    resizeReset();
}


ImageConfig Swapchain::imageConfig() const {
    //TEMP left off here. the swapchain selects the image format which is retarded, it should really be the surface that decides it
    debug_check(*this);

    return {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = _imageFormat,
        .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
    };
}


} // namespace cth
