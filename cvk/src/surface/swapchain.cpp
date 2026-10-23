#include "cvk/surface/swapchain.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/enumerate.hpp"
#include "cvk/utility/vk_exceptions.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <algorithm>
#include <ranges>


namespace cvk {
using std::span;
using std::vector;

Swapchain::Swapchain(Core const& core, Queue const& presentQueue, Surface const& surface, Config config) :
    _presentQueue{presentQueue}, _surface{&surface}, _core{&core}, _config{std::move(config)} {
    Core::debug_check(core);
    Surface::debug_check(surface);
    Queue::debug_check_present(presentQueue);
    Config::debug_check(_config);
}

Swapchain::Swapchain(
    Core const& core,
    Queue const& presentQueue,
    Surface const& surface,
    Config config,
    State const& state
) : Swapchain{core, presentQueue, surface, std::move(config)} { wrap(state); }

Swapchain::Swapchain(
    Core const& core,
    Queue const& presentQueue,
    Surface const& surface,
    Config config,
    VkExtent2D extent
) : Swapchain{core, presentQueue, surface, std::move(config)} { create(extent); }

void Swapchain::wrap(State const& state) {
    optDestroy();

    _generations.emplace_back(core_handle<Swapchain>{state.vkSwapchain.get(), core()});
    _extent = state.extent;
    _surfaceFormat = state.surfaceFormat;
    _presentMode = state.presentMode;

    createImages();
}

void Swapchain::create(VkExtent2D extent) {
    CTH_CRITICAL(extent.width == 0 || extent.height == 0, "extent width and height must be > 0, got {}", extent) {}
    optDestroy();

    auto const [surfaceFormat, presentMode] = pick(_config);
    _surfaceFormat = surfaceFormat;
    _presentMode = presentMode;
    createSwapchain(extent, VK_NULL_HANDLE);
}

void Swapchain::resize(VkExtent2D extent) {
    debug_check(*this);
    CTH_CRITICAL(extent.width == 0 || extent.height == 0, "extent width and height must be > 0, got {}", extent) {}

    unacquireAll();
    createSwapchain(extent, get());
}

void Swapchain::recreate(VkExtent2D extent, Config config) {
    debug_check(*this);
    CTH_CRITICAL(extent.width == 0 || extent.height == 0, "extent width and height must be > 0, got {}", extent) {}
    Config::debug_check(config);

    auto const [surfaceFormat, presentMode] = pick(config);

    unacquireAll();
    _config = std::move(config);
    _surfaceFormat = surfaceFormat;
    _presentMode = presentMode;
    createSwapchain(extent, get());
}

void Swapchain::destroy() {
    debug_check(*this);

    _generations.clear();

    forget();
}

Swapchain::State Swapchain::release() {
    debug_check(*this);

    State const state{
        .vkSwapchain = current().handle.release(),
        .extent = _extent,
        .surfaceFormat = _surfaceFormat,
        .presentMode = _presentMode,
    };
    // waits for the present fences, releases the images, the current generation has no handle to destroy
    _generations.clear();

    forget();
    return state;
}

Swapchain::Acquired Swapchain::acquire(Semaphore const& signal, uint64_t timeout) {
    debug_check(*this);
    Semaphore::debug_check(signal);

    collectRetired();

    uint32_t imageIndex = 0;
    auto const result = core().functions()->vkAcquireNextImageKHR(
        core().vkDevice(),
        get(),
        timeout,
        signal.get(),
        VK_NULL_HANDLE,
        &imageIndex
    );

    if(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        current().acquired[imageIndex] = true;
        return {result, imageIndex};
    }

    CVK_RESULT_STABLE_THROW(
        result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_TIMEOUT && result != VK_NOT_READY,
        result,
        "failed to acquire a swapchain image"
    ) {}

    return {result, std::nullopt};
}

VkResult Swapchain::present(uint32_t imageIndex) {
    debug_check(*this);
    CTH_CRITICAL(imageIndex >= imageCount() || !acquired(imageIndex), "image {} must be acquired", imageIndex) {}

    collectRetired();

    auto& generation = current();

    // signaled by the last present of the image or created signaled
    auto const& fence = generation.presentFences[imageIndex];
    fence.waitReset();

    VkFence const vkFence = fence.get();
    VkSwapchainPresentFenceInfoKHR const fenceInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
        .pNext = nullptr,
        .swapchainCount = 1,
        .pFences = &vkFence,
    };

    VkSemaphore const vkSemaphore = generation.presentSemaphores[imageIndex].get();
    VkSwapchainKHR const vkSwapchain = generation.handle.get();
    VkPresentInfoKHR const info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = &fenceInfo,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vkSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &vkSwapchain,
        .pImageIndices = &imageIndex,
        .pResults = nullptr,
    };

    auto const result = _presentQueue.rawPresent(info);
    generation.acquired[imageIndex] = false;
    return result;
}

void Swapchain::unacquire(span<uint32_t const> imageIndices) {
    debug_check(*this);
    for(auto const index : imageIndices)
        CTH_CRITICAL(index >= imageCount() || !acquired(index), "image {} must be acquired", index) {}

    if(imageIndices.empty()) return;

    VkReleaseSwapchainImagesInfoKHR const info{
        .sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_KHR,
        .pNext = nullptr,
        .swapchain = get(),
        .imageIndexCount = static_cast<uint32_t>(imageIndices.size()),
        .pImageIndices = imageIndices.data(),
    };
    auto const result = core().functions()->vkReleaseSwapchainImagesKHR(core().vkDevice(), &info);

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to unacquire swapchain images") {}

    for(auto const index : imageIndices)
        current().acquired[index] = false;
}

void Swapchain::destroy(DeviceTable table, VkSwapchainKHR vkSwapchain) {
    CTH_WARN(vkSwapchain == VK_NULL_HANDLE, "vkSwapchain invalid (VK_NULL_HANDLE)") {}

    table->vkDestroySwapchainKHR(table.device(), vkSwapchain, nullptr);
}

Swapchain::Generation::~Generation() {
    for(auto const& fence : presentFences)
        fence.wait();

    // the swapchain destroys its images
    for(auto& image : images)
        image.release();
}

std::pair<VkSurfaceFormatKHR, VkPresentModeKHR> Swapchain::pick(Config const& config) const {
    auto const& physicalDevice = core().physicalDevice();

    auto const formats = _surface->formats(physicalDevice);
    auto const& allowedFormats = config.formats.empty() ? formats : config.formats;
    auto const formatIt = std::ranges::find_first_of(allowedFormats, formats);
    CVK_STABLE_THROW(
        formatIt == allowedFormats.end(),
        "the surface offers no allowed format, allowed: {}, offered: {}",
        config.formats,
        formats
    ) {}

    if(config.presentModes.empty()) return {*formatIt, VK_PRESENT_MODE_FIFO_KHR};

    auto const presentModes = _surface->presentModes(physicalDevice);
    auto const presentModeIt = std::ranges::find_first_of(config.presentModes, presentModes);
    CVK_STABLE_THROW(
        presentModeIt == config.presentModes.end(),
        "the surface offers no allowed present mode, allowed: {}, offered: {}",
        config.presentModes,
        presentModes
    ) {}

    return {*formatIt, *presentModeIt};
}

void Swapchain::createSwapchain(VkExtent2D extent, VkSwapchainKHR oldSwapchain) {
    Surface::debug_check(*_surface);

    auto const capabilities = _surface->capabilities(core().physicalDevice());
    auto const imageExtent = evalExtent(extent, capabilities);

    auto const info = _config.createInfo(
        _surface->get(),
        _surfaceFormat,
        _presentMode,
        //TODO override of the pre transform, for pre-rotation on rotated mobile displays
        capabilities.currentTransform,
        imageExtent,
        evalImageCount(_config.minImageCount, capabilities),
        oldSwapchain
    );

    VkSwapchainKHR ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateSwapchainKHR(core().vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create swapchain") {
        // the old swapchain retires also when the creation fails, no generation stays current
        _generations.clear();
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _generations.emplace_back(core_handle<Swapchain>{ptr, core()});
    _extent = imageExtent;

    createImages();
}

void Swapchain::createImages() {
    auto& generation = current();

    auto const vkImages = dev::enumerate(
        "swapchain images",
        core().functions()->vkGetSwapchainImagesKHR,
        core().vkDevice(),
        generation.handle.get()
    );
    auto const count = static_cast<uint32_t>(vkImages.size());

    Image::Config const imageConfig{
        .format = _surfaceFormat.format,
        .usage = _config.usage,
        .queueFamilies = _config.queueFamilies,
    };

    generation.images.reserve(count);
    for(auto const vkImage : vkImages)
        generation.images.emplace_back(core(), imageConfig, Image::State{.vkImage = vkImage, .extent = _extent});

    generation.views.reserve(count);
    for(auto const& image : generation.images)
        generation.views.emplace_back(image, ImageView::Config{}, cvk::create);

    generation.presentSemaphores.reserve(count);
    generation.presentFences.reserve(count);
    for(uint32_t index = 0; index < count; ++index) {
        generation.presentSemaphores.emplace_back(core(), cvk::create);
        generation.presentFences.emplace_back(core(), cvk::create, VK_FENCE_CREATE_SIGNALED_BIT);
    }

    generation.acquired.assign(count, false);
}

void Swapchain::unacquireAll() {
    vector<uint32_t> acquiredImages{};
    for(uint32_t index = 0; index < imageCount(); ++index)
        if(acquired(index)) acquiredImages.push_back(index);
    unacquire(acquiredImages);
}

void Swapchain::collectRetired() {
    if(_generations.size() < 2) return;

    // the generations have no move assignment, the kept ones move into a new vector
    vector<Generation> kept{};
    kept.reserve(_generations.size());
    for(auto& generation : _generations | std::views::take(_generations.size() - 1))
        if(!std::ranges::all_of(generation.presentFences, &Fence::signaled)) kept.push_back(std::move(generation));
    kept.push_back(std::move(current()));

    _generations = std::move(kept);
}

void Swapchain::forget() {
    _extent = {};
    _surfaceFormat = {};
    _presentMode = VK_PRESENT_MODE_FIFO_KHR;
}
}
