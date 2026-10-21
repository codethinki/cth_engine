#include "cvk/surface/swapchain.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
using std::span;
using std::vector;

Swapchain::Swapchain(Core const& core, Queue const& presentQueue, Surface const& surface, Config config) :
    _presentQueue{presentQueue}, _surface{&surface}, _config{std::move(config)}, _handle{core} {
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

    _handle.reset(state.vkSwapchain.get(), core());
    _extent = state.extent;
    _surfaceFormat = state.surfaceFormat;
    _presentMode = state.presentMode;

    createImages();
}

void Swapchain::create(VkExtent2D extent) {
    optDestroy();

    createSwapchain(extent, VK_NULL_HANDLE);
}

void Swapchain::resize(VkExtent2D extent) {
    debug_check(*this);

    vector<uint32_t> acquiredImages{};
    for(uint32_t index = 0; index < imageCount(); ++index)
        if(_acquired[index]) acquiredImages.push_back(index);
    unacquire(acquiredImages);

    // the swapchain destroys its images
    for(auto& image : _images)
        image.release();

    auto const oldSwapchain = get();
    _retired.push_back(
        Retired{
            .handle = std::move(_handle),
            .images = std::move(_images),
            .views = std::move(_views),
            .presentSemaphores = std::move(_presentSemaphores),
            .presentFences = std::move(_presentFences),
        }
    );
    _images.clear();
    _views.clear();
    _presentSemaphores.clear();
    _presentFences.clear();
    _acquired.clear();

    createSwapchain(extent, oldSwapchain);
}

void Swapchain::destroy() {
    debug_check(*this);

    destroyImages();
    destroyRetired();
    _handle.reset();

    forget();
}

Swapchain::State Swapchain::release() {
    debug_check(*this);

    destroyImages();
    destroyRetired();

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
    CTH_CRITICAL(imageIndex >= imageCount() || !_acquired[imageIndex], "image {} must be acquired", imageIndex) {}

    collectRetired();

    // signaled by the last present of the image or created signaled
    auto const& fence = _presentFences[imageIndex];
    fence.waitReset();

    VkFence const vkFence = fence.get();
    VkSwapchainPresentFenceInfoKHR const fenceInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
        .pNext = nullptr,
        .swapchainCount = 1,
        .pFences = &vkFence,
    };

    VkSemaphore const vkSemaphore = _presentSemaphores[imageIndex].get();
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
        CTH_CRITICAL(index >= imageCount() || !_acquired[index], "image {} must be acquired", index) {}

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
        _acquired[index] = false;
}

void Swapchain::destroy(DeviceTable table, VkSwapchainKHR vkSwapchain) {
    CTH_WARN(vkSwapchain == VK_NULL_HANDLE, "vkSwapchain invalid (VK_NULL_HANDLE)") {}

    table->vkDestroySwapchainKHR(table.device(), vkSwapchain, nullptr);
}

void Swapchain::createSwapchain(VkExtent2D extent, VkSwapchainKHR oldSwapchain) {
    Surface::debug_check(*_surface);
    CTH_CRITICAL(extent.width == 0 || extent.height == 0, "extent width and height must be > 0") {}

    auto const& physicalDevice = core().physicalDevice();
    auto const capabilities = _surface->capabilities(physicalDevice);
    auto const surfaceFormat = _surface->format(physicalDevice);
    auto const presentMode = _surface->presentMode(physicalDevice);
    auto const imageExtent = evalExtent(extent, capabilities);

    auto const info = _config.createInfo(
        _surface->get(),
        surfaceFormat,
        presentMode,
        capabilities.currentTransform,
        imageExtent,
        evalImageCount(_config.minImageCount, capabilities),
        oldSwapchain
    );

    VkSwapchainKHR ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateSwapchainKHR(core().vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create swapchain") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
    _extent = imageExtent;
    _surfaceFormat = surfaceFormat;
    _presentMode = presentMode;

    createImages();
}

void Swapchain::createImages() {
    auto const* functions = core().functions();

    uint32_t count = 0;
    auto const countResult = functions->vkGetSwapchainImagesKHR(core().vkDevice(), get(), &count, nullptr);
    CVK_RESULT_STABLE_THROW(countResult != VK_SUCCESS, countResult, "failed to get the swapchain image count") {}

    vector<VkImage> vkImages(count);
    auto const imagesResult = functions->vkGetSwapchainImagesKHR(core().vkDevice(), get(), &count, vkImages.data());
    CVK_RESULT_STABLE_THROW(imagesResult != VK_SUCCESS, imagesResult, "failed to get the swapchain images") {}

    Image::Config const imageConfig{
        .format = _surfaceFormat.format,
        .usage = _config.usage,
        .queueFamilies = _config.queueFamilies,
    };

    _images.reserve(count);
    for(auto const vkImage : vkImages)
        _images.emplace_back(core(), imageConfig, Image::State{.vkImage = vkImage, .extent = _extent});

    _views.reserve(count);
    for(auto const& image : _images)
        _views.emplace_back(image, ImageView::Config{}, cvk::create);

    _presentSemaphores.reserve(count);
    _presentFences.reserve(count);
    for(uint32_t index = 0; index < count; ++index) {
        _presentSemaphores.emplace_back(core(), cvk::create);
        _presentFences.emplace_back(core(), cvk::create, VK_FENCE_CREATE_SIGNALED_BIT);
    }

    _acquired.assign(count, false);
}

void Swapchain::destroyImages() {
    for(auto const& fence : _presentFences)
        fence.wait();

    _views.clear();
    // the swapchain destroys its images
    for(auto& image : _images)
        image.release();
    _images.clear();

    _presentSemaphores.clear();
    _presentFences.clear();
    _acquired.clear();
}

void Swapchain::collectRetired() {
    std::erase_if(_retired, [](Retired const& retired) {
        return std::ranges::all_of(retired.presentFences, &Fence::signaled);
    });
}

void Swapchain::destroyRetired() {
    for(auto const& retired : _retired)
        for(auto const& fence : retired.presentFences)
            fence.wait();

    _retired.clear();
}

void Swapchain::forget() {
    _extent = {};
    _surfaceFormat = {};
    _presentMode = VK_PRESENT_MODE_FIFO_KHR;
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
    if(this == &other) return *this;

    optDestroy();
    destroyRetired();

    _presentQueue = other._presentQueue;
    _surface = other._surface;
    _config = std::move(other._config);
    _handle = std::move(other._handle);
    _images = std::move(other._images);
    _views = std::move(other._views);
    _presentSemaphores = std::move(other._presentSemaphores);
    _presentFences = std::move(other._presentFences);
    _acquired = std::move(other._acquired);
    _retired = std::move(other._retired);
    _extent = other._extent;
    _surfaceFormat = other._surfaceFormat;
    _presentMode = other._presentMode;

    other.forget();
    return *this;
}
}
