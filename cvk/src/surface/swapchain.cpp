#include "cvk/surface/swapchain.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constant/os_constants.hpp"
#include "cvk/utility/enumerate.hpp"
#include "cvk/utility/vk_exceptions.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <algorithm>


namespace cvk {
using std::span;
using std::vector;

Swapchain::Swapchain(
    Core const& core,
    Queue const& presentQueue,
    Surface const& surface,
    Config config
) :
    _presentQueue{presentQueue},
    _surface{surface},
    _config{std::move(config)},
    _handle{core} { Config::debug_check(_config); }

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
    VkExtent2D extent,
    Swapchain const* oldSwapchainPtr
) : Swapchain{core, presentQueue, surface, std::move(config)} { create(extent, oldSwapchainPtr); }

void Swapchain::wrap(State const& state) {
    Core::debug_check(core());
    Surface::debug_check(*_surface);
    Queue::debug_check_present(_presentQueue);

    optDestroy();

    _handle.reset(state.vkSwapchain.get(), core());
    _extent = state.extent;
    _surfaceFormat = state.surfaceFormat;
    _presentMode = state.presentMode;

    createImages();
}

void Swapchain::create(VkExtent2D extent, Swapchain const* oldSwapchainPtr) {
    Core::debug_check(core());
    Surface::debug_check(*_surface);
    Queue::debug_check_present(_presentQueue);
    CTH_CRITICAL(
        extent.width == 0 || extent.height == 0,
        "extent width and height must be > 0, got {}",
        extent
    ) {}
    CTH_CRITICAL(oldSwapchainPtr == this, "the old swapchain must be another swapchain") {}
    if(oldSwapchainPtr != nullptr) {
        debug_check(*oldSwapchainPtr);
        CTH_CRITICAL(
            &*oldSwapchainPtr->_surface != &*_surface,
            "the old swapchain must be of the same surface"
        ) {}
    }
    optDestroy();

    auto const [surfaceFormat, presentMode] = pick(_config);
    auto const capabilities = _surface->capabilities(core().physicalDevice());
    auto const imageExtent = evalExtent(extent, capabilities);

    auto const info = _config.createInfo(
        _surface->get(),
        surfaceFormat,
        presentMode,
        //TODO override of the pre transform, for pre-rotation on rotated mobile displays
        capabilities.currentTransform,
        imageExtent,
        evalImageCount(_config.minImageCount, capabilities),
        oldSwapchainPtr != nullptr ? oldSwapchainPtr->get() : VK_NULL_HANDLE
    );

    VkSwapchainKHR ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateSwapchainKHR(
        core().vkDevice(),
        &info,
        nullptr,
        &ptr
    );
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to create swapchain") {}

    _handle.reset(ptr, core());
    _extent = imageExtent;
    _surfaceFormat = surfaceFormat;
    _presentMode = presentMode;

    try { createImages(); }
    catch(...) {
        destroy();
        throw;
    }
}

void Swapchain::destroy() {
    debug_check(*this);

    // the teardown order of the members: the presents end, the images release, the swapchain destroys them
    _presentFences.clear();
    _presentSemaphores.clear();
    _views.clear();
    _images.clear();
    _acquired.clear();
    _handle.reset();

    forget();
}

Swapchain::State Swapchain::release() {
    debug_check(*this);

    _presentFences.clear();
    _presentSemaphores.clear();
    _views.clear();
    _images.clear();
    _acquired.clear();

    State const state{
        .vkSwapchain = _handle.release(),
        .extent = _extent,
        .surfaceFormat = _surfaceFormat,
        .presentMode = _presentMode,
    };

    forget();
    return state;
}

Swapchain::Acquired Swapchain::acquire(Semaphore const& signal, uint64_t timeout) {
    debug_check(*this);
    Semaphore::debug_check(signal);

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
        _acquired[imageIndex] = true;
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
    CTH_CRITICAL(
        imageIndex >= imageCount() || !acquired(imageIndex),
        "image {} must be acquired",
        imageIndex
    ) {}

    // signaled by the last present of the image or created signaled
    auto const& fence = presentFence(imageIndex);
    fence.waitReset();

    VkFence const vkFence = fence.get();
    VkSwapchainPresentFenceInfoKHR const fenceInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
        .pNext = nullptr,
        .swapchainCount = 1,
        .pFences = &vkFence,
    };

    VkSemaphore const vkSemaphore = presentSemaphore(imageIndex).get();
    VkSwapchainKHR const vkSwapchain = get();
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
    _acquired[imageIndex] = false;
    return result;
}

void Swapchain::unacquire(span<uint32_t const> imageIndices) {
    debug_check(*this);
    for(auto const index : imageIndices)
        CTH_CRITICAL(
        index >= imageCount() || !acquired(index),
        "image {} must be acquired",
        index
    ) {}

    if(imageIndices.empty())
        return;

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
        _acquired[index] = false;
}

bool Swapchain::presented() const {
    debug_check(*this);

    return std::ranges::all_of(
        _presentFences,
        [](WaitingFence const& presentFence) { return presentFence.fence.signaled(); }
    );
}

void destroy(DeviceTable table, VkSwapchainKHR vkSwapchain) {
    CTH_WARN(vkSwapchain == VK_NULL_HANDLE, "vkSwapchain invalid (VK_NULL_HANDLE)") {}

    table->vkDestroySwapchainKHR(table.device(), vkSwapchain, nullptr);
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

    if(config.presentModes.empty())
        return {*formatIt, VK_PRESENT_MODE_FIFO_KHR};

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

void Swapchain::createImages() {
    auto const vkImages = dev::enumerate(
        "swapchain images",
        core().functions()->vkGetSwapchainImagesKHR,
        core().vkDevice(),
        get()
    );
    auto const count = static_cast<uint32_t>(vkImages.size());

    Image::Config const imageConfig{
        .format = _surfaceFormat.format,
        .usage = _config.usage,
        .queueFamilies = _config.queueFamilies,
    };

    _images.reserve(count);
    for(auto const vkImage : vkImages)
        _images.emplace_back(
            Image{core(), imageConfig, Image::State{.vkImage = vkImage, .extent = _extent}}
        );

    _views.reserve(count);
    for(auto const& [image] : _images)
        _views.emplace_back(image, ImageView::Config{}, cvk::create);

    _presentSemaphores.reserve(count);
    _presentFences.reserve(count);
    for(uint32_t i = 0; i < count; ++i) {
        _presentSemaphores.emplace_back(core(), cvk::create);
        _presentFences.emplace_back(
            Fence{
                core(),
                cvk::create,
                VK_FENCE_CREATE_SIGNALED_BIT,
                constants::OS_FENCE_HANDLE_TYPE
            }
        );
    }

    _acquired.assign(count, false);
}

void Swapchain::forget() {
    _extent = {};
    _surfaceFormat = {};
    _presentMode = VK_PRESENT_MODE_FIFO_KHR;
}
}
