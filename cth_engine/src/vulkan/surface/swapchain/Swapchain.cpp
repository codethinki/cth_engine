#include "Swapchain.hpp"

#include "../graphics_core/CthGraphicsSyncConfig.hpp"
#include "src/vulkan/base/CthCore.hpp"
#include "src/vulkan/base/CthDevice.hpp"
#include "src/vulkan/base/CthPhysicalDevice.hpp"
#include "src/vulkan/render/cmd/CthCmdBuffer.hpp"
#include "src/vulkan/render/control/CthFence.hpp"
#include "src/vulkan/render/control/CthPipelineBarrier.hpp"
#include "src/vulkan/render/control/CthSemaphore.hpp"
#include "src/vulkan/render/pass/attachment/AttachmentCollection.hpp"
#include "src/vulkan/render/pass/framebuffer/Framebuffer.hpp"
#include "src/vulkan/resource/CthDestructionQueue.hpp"
#include "src/vulkan/surface/CthSurface.hpp"
#include "src/vulkan/utility/cth_vk_exceptions.hpp"
#include "src/vulkan/utility/cth_vk_overloads.hpp"


namespace cth::vk {

Swapchain::Swapchain(Core const& core, Queue const& present_queue, GraphicsSyncConfig const& sync_config, Surface const& surface) :
    _core(&core), _presentQueue(&present_queue), _surface{&surface}, _syncConfig(&sync_config) {
    init();
}
Swapchain::Swapchain(Core const& core, Queue const& present_queue,
    GraphicsSyncConfig const& sync_config, Surface const& surface, VkExtent2D window_extent) : Swapchain{core, present_queue, sync_config, surface} {

    create(window_extent);
}
Swapchain::~Swapchain() {
    optDestroy();
    Swapchain::debug_check_leak(this);
}

void Swapchain::create(VkExtent2D window_extent, VkSwapchainKHR old_swapchain) {
    Core::debug_check(*_core);
    Surface::debug_check(*_surface);
    Queue::debug_check_present(*_presentQueue);
    GraphicsSyncConfig::debug_check(*_syncConfig);

    optDestroy();


    _imageIndices.fill(NO_IMAGE_INDEX);
    _msaaSamples = evalMsaaSampleCount();

    createSyncObjects();

    createSwapchain(window_extent, old_swapchain);

    createResolveAttachments();

    createPresentInfos();
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



VkResult Swapchain::acquireNextImage() {
    //TODO add timeout
    auto const pulse = _syncConfig->pulseVal();

    auto const& fence = _imageAvailableFences[pulse];
    auto const semaphore = _syncConfig->imageAvailableSemaphore(pulse)->get();

    fence.wait();
    fence.reset();

    VkResult const acquireResult = _core->functions()->vkAcquireNextImageKHR(
        _core->vkDevice(),
        _handle.get(),
        std::numeric_limits<uint64_t>::max(),
        semaphore,
        fence.get(),
        &_imageIndices[pulse]
    );

    CTH_STABLE_ERR(acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR, "failed to acquire vk_image")
        throw cth::vk::result_exception{acquireResult, details->exception()};

    return acquireResult;
}

void Swapchain::skipAcquire() const {
    auto const pulse = _syncConfig->pulseVal();
    auto const& fence = _imageAvailableFences[pulse];
    fence.wait();
    fence.reset();

    auto const semaphore = _syncConfig->imageAvailableSemaphore(pulse)->get();


    auto const submitInfo = VkSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 0,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &semaphore
    };

    auto const result = _core->functions()->vkQueueSubmit(_presentQueue->get(), 1, &submitInfo, fence.get());
    //TODO this should be done via Queue::skip()

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to skip-acquire a vk_image")
        throw cth::vk::result_exception{result, details->exception()};
}


VkResult Swapchain::present() {
    size_t const pulse = _syncConfig->pulseVal();

    auto& imageIndex = _imageIndices[pulse];

    CTH_CRITICAL(imageIndex == NO_IMAGE_INDEX, "no acquired vk_image available") { details->add("frame: ({})", pulse); }

    auto const result = _presentQueue->present(imageIndex, _presentInfos[pulse]);

    imageIndex = NO_IMAGE_INDEX;

    return result;
}

void Swapchain::skipPresent() {
    auto const pulse = _syncConfig->pulseVal();

    auto& imageIndex = _imageIndices[pulse];

    CTH_WARN(imageIndex != NO_IMAGE_INDEX, "skip presenting an acquired vk_image, it will be discarded") {
        int x = 0;
    }

    _presentQueue->const_skip(_presentInfos[pulse]);

    imageIndex = NO_IMAGE_INDEX;
}

void Swapchain::changeSwapchainImageQueue(uint32_t release_queue, CmdBuffer const& release_cmd_buffer, uint32_t acquire_queue,
    CmdBuffer const& acquire_cmd_buffer, uint32_t image_index) const {
    //TEMP test this function
    std::unordered_map<Image*, ImageBarrier::Info> const images{
        {_resolveAttachments->image(image_index), ImageBarrier::Info::QueueTransition(0, release_queue, 0, acquire_queue)}
    };

    ImageBarrier releaseBarrier{core(), {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT}, images};
    releaseBarrier.execute(release_cmd_buffer);

    ImageBarrier barrier{core(), {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT}, images};
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



void Swapchain::initSyncObjects() {
    _imageAvailableFences.reserve(constants::FRAMES_IN_FLIGHT);
    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++)
        _imageAvailableFences.emplace_back(*_core);
}
void Swapchain::initAttachments() {
    
}

void Swapchain::init() { initSyncObjects(); }

void Swapchain::setImageFormat(VkFormat format) {
    _imageFormat = format;
    CTH_CRITICAL(_imageFormat == VK_FORMAT_UNDEFINED, "image format must not be VK_FORMAT_UNDEFINED") {}
}
VkSampleCountFlagBits Swapchain::evalMsaaSampleCount() const {
    uint32_t const maxSamples = _core->physicalDevice().maxSampleCount() / 2; //TODO add proper max_sample_count selection

    uint32_t samples = 1;
    while(samples < maxSamples && samples < constants::MAX_MSAA_SAMPLES) samples *= 2;

    return static_cast<VkSampleCountFlagBits>(samples);
}

void Swapchain::createSyncObjects() {
    for(auto& fence : _imageAvailableFences)
        fence.create(VK_FENCE_CREATE_SIGNALED_BIT);
}

VkExtent2D Swapchain::chooseSwapExtent(VkExtent2D window_extent, VkSurfaceCapabilitiesKHR const& capabilities) {
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;

    VkExtent2D const extent{
        .width = std::max(capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, window_extent.width)),
        .height = std::max(capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, window_extent.height))
    };

    return extent;
}
uint32_t Swapchain::evalMinImageCount(uint32_t min, uint32_t max) {
    uint32_t imageCount = min + 1; //TODO check if this is really wrong if the imageCount is 4
    if(max > 0 && imageCount > max) imageCount = max;


    log::msg<except::INFO>("vk_image count: {0}, frames in flight: {1}", min + 1, constants::FRAMES_IN_FLIGHT);

    return imageCount;
}

VkSwapchainCreateInfoKHR Swapchain::createInfo(VkSurfaceKHR surface,
    VkSurfaceFormatKHR surface_format, VkSurfaceCapabilitiesKHR const& capabilities,
    VkPresentModeKHR present_mode, VkExtent2D extent, uint32_t image_count,
    VkSwapchainKHR old_swapchain) {

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

void Swapchain::createSwapchain(VkExtent2D window_extent, VkSwapchainKHR old_swapchain) {
    Swapchain::debug_check_window_extent(window_extent);

    _windowExtent = window_extent;

    auto const capabilities = _surface->capabilities(_core->physicalDevice());

    auto const surfaceFormat = _surface->format(_core->physicalDevice());
    auto const presentMode = _surface->presentMode(_core->physicalDevice());

    auto const extent = chooseSwapExtent(window_extent, capabilities);

    uint32_t const imageCount = evalMinImageCount(capabilities.minImageCount, capabilities.maxImageCount);

    auto const info = createInfo(_surface->get(), surfaceFormat, capabilities, presentMode, extent, imageCount, old_swapchain);


    VkSwapchainKHR ptr = nullptr;
    auto const createResult = _core->deviceTable()->vkCreateSwapchainKHR(_core->vkDevice(), &info, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create swapchain")
        throw cth::vk::result_exception{createResult, details->exception()};

    _handle = ptr;


    setImageFormat(surfaceFormat.format);

    _extent = extent;
    _aspectRatio = static_cast<float>(_extent.width) / static_cast<float>(_extent.height);
}



Image::Config Swapchain::createColorImageConfig(VkSampleCountFlagBits samples) const {
    //TEMP moved to frame resources in demo
    CTH_CRITICAL(_imageFormat == VK_FORMAT_UNDEFINED, "image format must not be VK_FORMAT_UNDEFINED") {}

    return Image::Config{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = _imageFormat,
        .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .samples = samples,
    };
}


auto Swapchain::getSwapchainImages() -> std::vector<std::unique_ptr<Image>> {
    uint32_t imageCount; //only min specified, might be higher
    auto const countResult = _core->deviceTable()->vkGetSwapchainImagesKHR(_core->vkDevice(), _handle.get(), &imageCount, nullptr);

    CTH_STABLE_ERR(countResult != VK_SUCCESS, "failed to get swapchain image count")
        throw cth::vk::result_exception{countResult, details->exception()};

    _imageCount = imageCount;

    std::vector<VkImage> vkImages{imageCount};
    auto const getResult = _core->deviceTable()->vkGetSwapchainImagesKHR(_core->vkDevice(), _handle.get(), &imageCount, vkImages.data());

    CTH_STABLE_ERR(getResult != VK_SUCCESS, "failed to get swapchain images")
        throw cth::vk::result_exception{getResult, details->exception()};


    std::vector<std::unique_ptr<Image>> images{};
    images.reserve(imageCount);

    auto const imageConfig = createColorImageConfig(VK_SAMPLE_COUNT_1_BIT);
    for(auto const& vkImage : vkImages)
        images.emplace_back(std::make_unique<Image>(*_core, imageConfig, Image::State{_extent, vkImage, true, nullptr}));

    return images;
}



void Swapchain::createResolveAttachments() {
    auto swapchainImages = getSwapchainImages();

    AttachmentDescription const description{
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .referenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    AttachmentCollection::State state{_extent};

    for(auto& image : swapchainImages) state.images.emplace_back(std::move(image));


    _resolveAttachments = std::make_unique<AttachmentCollection>(
        *_core,
        AttachmentCollection::Config{size(), constants::SWAPCHAIN_ATTACHMENT_INDEX, state.images[0]->config(), description}, 
        std::move(state)
    );
}



void Swapchain::createPresentInfos() {
    _presentInfos.reserve(constants::FRAMES_IN_FLIGHT);

    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++) {
        std::vector<Semaphore const*> semaphores{_syncConfig->renderFinishedSemaphore(i)};
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

    if(queue) queue->push(lambda);
    else lambda();
}

void Swapchain::destroySyncObjects() { for(auto& fence : _imageAvailableFences) fence.destroy(); }
void Swapchain::resizeReset() {
    _extent = {};
    _windowExtent = {};
    _aspectRatio = 0;
    _imageFormat = VK_FORMAT_UNDEFINED;
    _imageCount = 0;
    _imageIndices.fill(NO_IMAGE_INDEX);
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

