#pragma once
#include "swapchain_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/base/queue/queue.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>


namespace cvk {
class Core;
class Surface;

/**
 * `VkSwapchainKHR` wrapper
 * @details
 * - owns its images, their views, a present semaphore and a present fence per image
 * - the present fences export `constants::OS_FENCE_HANDLE_TYPE`
 * - a recreation is a new swapchain, created with the old one, see `create()`
 * @attention not thread safe
 */
class Swapchain {
public:
    using Config = SwapchainConfig;
    struct State;

    /**
     * result of `acquire()`
     */
    struct Acquired {
        /**
         * `VK_SUCCESS`, `VK_SUBOPTIMAL_KHR`, `VK_ERROR_OUT_OF_DATE_KHR`, `VK_TIMEOUT` or `VK_NOT_READY`
         */
        VkResult result;

        /** `std::nullopt` -> no image acquired, the semaphore stays unsignaled */
        std::optional<uint32_t> imageIndex;
    };

    /**
     * base constructor
     */
    Swapchain(Core const&, Queue const& presentQueue, Surface const&, Config);

    /**
     * constructs and wraps
     * @details calls @ref Swapchain(Core const&, Queue const&, Surface const&, Config), `wrap()`
     */
    Swapchain(Core const&, Queue const& presentQueue, Surface const&, Config, State const&);

    /**
     * constructs and creates
     * @details calls @ref Swapchain(Core const&, Queue const&, Surface const&, Config), `create()`
     */
    Swapchain(
        Core const&,
        Queue const& presentQueue,
        Surface const&,
        Config,
        VkExtent2D extent,
        Swapchain const* oldSwapchainPtr = nullptr
    );

    /**
     * @note calls `optDestroy()`
     */
    ~Swapchain() { optDestroy(); }

    /**
     * wraps the state, gets the images, creates their views, present semaphores and present fences
     * @details the format and the present mode come from the state, not from the config
     * @pre `Core::created()`, `Surface::created()`
     * @pre the queue can present to the surface
     * @pre the device enables `constants::PRESENT_DEVICE_EXTENSIONS`, a present queue of the core does
     * @pre the swapchain was created for the surface with the config and the state
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkGetSwapchainImagesKHR()`
     */
    void wrap(State const&);

    /**
     * picks the format and the present mode of the config, creates the swapchain
     * @param extent of the window, a fixed `currentExtent` of the surface wins
     * @param oldSwapchainPtr the swapchain that this one replaces, `nullptr` -> none
     * @details the first entries of `Config::formats` and `Config::presentModes` that the surface offers win
     * @details an old swapchain retires, also on a failure: its acquired images still present, no
     *  acquire on it (VUID-vkAcquireNextImageKHR-swapchain-01285), see `vkCreateSwapchainKHR()`
     * @pre `Core::created()`, `Surface::created()`
     * @pre the queue can present to the surface
     * @pre the device enables `constants::PRESENT_DEVICE_EXTENSIONS`, a present queue of the core does
     * @pre `extent` width and height > 0
     * @pre `oldSwapchainPtr` -> another created swapchain of the surface
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of `vkCreateSwapchainKHR()` or `vkGetSwapchainImagesKHR()`
     */
    void create(VkExtent2D extent, Swapchain const* oldSwapchainPtr = nullptr);

    /**
     * destroys the swapchain and forgets
     * @details destroys the views, present semaphores and present fences, the swapchain destroys its images
     * @pre `created()`
     * @pre no present fence is pending: the presentation engine took its reference to the payloads
     * @pre the submits that signal the present semaphores completed:
     *  VUID-vkDestroySemaphore-semaphore-05149
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * releases ownership, returns the state and forgets
     * @details destroys the views, present semaphores and present fences
     * @pre `created()`
     * @pre the preconditions of `destroy()` for the present fences and semaphores
     */
    State release();

    /**
     * acquires the next image
     * @param signal signaled when the image is ready
     * @param timeout in nanoseconds
     * @pre `created()`
     * @pre `signal` is unsignaled and has no pending operation
     * @throws cvk::vk_result_exception other result of `vkAcquireNextImageKHR()`
     */
    [[nodiscard]] Acquired acquire(
        Semaphore const& signal,
        uint64_t timeout = std::numeric_limits<uint64_t>::max()
    );

    /**
     * presents the image once its present semaphore signals
     * @return `VK_SUCCESS`, `VK_SUBOPTIMAL_KHR` or `VK_ERROR_OUT_OF_DATE_KHR`
     * @details resets the present fence of the image, the present signals it, see `presentFence()`
     * @details `VK_ERROR_OUT_OF_DATE_KHR` -> the present still waits for the semaphore
     * @pre the image is acquired
     * @pre the present fence of the image is not pending: signaled, or reset by a present that was
     *  not enqueued, see `vkQueuePresentKHR()`
     * @pre a submit signals `presentSemaphore()` of the image
     * @throws cvk::vk_result_exception other result of `vkQueuePresentKHR()`
     */
    [[nodiscard]] VkResult present(uint32_t imageIndex);

    /**
     * gives acquired images back without a present
     * @pre the images are acquired and all uses of them completed
     * @attention the acquire semaphores of the images still signal, wait for them before reuse
     * @throws cvk::vk_result_exception result of `vkReleaseSwapchainImagesKHR()`
     */
    void unacquire(std::span<uint32_t const> imageIndices);

    /**
     * @return the extent clamped to the limits of the surface, a fixed `currentExtent` wins
     */
    [[nodiscard]] static constexpr VkExtent2D evalExtent(
        VkExtent2D extent,
        VkSurfaceCapabilitiesKHR const&
    );

    /**
     * @param minImageCount `0` -> `minImageCount` of the surface + 1
     * @return the count clamped to the limits of the surface (`maxImageCount` `0` => no limit)
     */
    [[nodiscard]] static constexpr uint32_t evalImageCount(
        uint32_t minImageCount,
        VkSurfaceCapabilitiesKHR const&
    );

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkSwapchainKHR get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }
    [[nodiscard]] constexpr Surface const& surface() const { return *_surface; }

    [[nodiscard]] constexpr VkExtent2D extent() const { return _extent; }
    [[nodiscard]] constexpr VkFormat format() const { return _surfaceFormat.format; }
    [[nodiscard]] constexpr VkColorSpaceKHR colorSpace() const { return _surfaceFormat.colorSpace; }
    [[nodiscard]] constexpr VkPresentModeKHR presentMode() const { return _presentMode; }
    [[nodiscard]] constexpr uint32_t imageCount() const {
        return static_cast<uint32_t>(_images.size());
    }

    /**
     * @return width / height
     * @pre `created()`
     */
    [[nodiscard]] constexpr float aspectRatio() const {
        return static_cast<float>(_extent.width) / static_cast<float>(_extent.height);
    }

    /**
     * @pre `imageIndex < imageCount()`
     */
    [[nodiscard]] constexpr Image const& image(uint32_t imageIndex) const {
        return _images[imageIndex].image;
    }

    /**
     * @pre `imageIndex < imageCount()`
     */
    [[nodiscard]] constexpr ImageView const& view(uint32_t imageIndex) const {
        return _views[imageIndex];
    }

    /**
     * @return the semaphore that `present()` of the image waits for, a submit signals it
     * @pre `imageIndex < imageCount()`
     */
    [[nodiscard]] constexpr Semaphore const& presentSemaphore(uint32_t imageIndex) const {
        return _presentSemaphores[imageIndex];
    }

    /**
     * @return the fence that the last present of the image signals, created signaled
     * @details exportable with `constants::OS_FENCE_HANDLE_TYPE`
     * @details the swapchain waits on none of them: the caller knows which ones are pending
     * @pre `imageIndex < imageCount()`
     * @see `Fence::extractOsHandle()`
     */
    [[nodiscard]] constexpr Fence const& presentFence(uint32_t imageIndex) const {
        return _presentFences[imageIndex];
    }

private:
    // the element wrapper: = default moves of the swapchain tear down in the member order, the
    // swapchain destroys its images itself
    /**
     * the destruction releases the image, it does not destroy it: the swapchain destroys its images
     */
    struct ReleasingImage {
        explicit ReleasingImage(Image image) : image{std::move(image)} {}
        ~ReleasingImage() {
            if(image.created())
                static_cast<void>(image.release());
        }

        Image image;

        // the user-declared destructor suppresses the implicit moves
        ReleasingImage(ReleasingImage&& other) noexcept = default;
        ReleasingImage& operator=(ReleasingImage&& other) noexcept = default;
    };

    [[nodiscard]] std::pair<VkSurfaceFormatKHR, VkPresentModeKHR> pick(Config const&) const;

    void createImages();

    void forget();

    Queue _presentQueue;
    cth::ref_wrapper<Surface const> _surface;
    Config _config;

    // the objects of the images before the handle, in the order of their teardown: = default moves destroy the
    // fences and semaphores, release the images, destroy the views, then the swapchain
    std::vector<Fence> _presentFences{};
    std::vector<Semaphore> _presentSemaphores{};
    std::vector<ImageView> _views{};
    std::vector<ReleasingImage> _images{};

    core_handle<VkSwapchainKHR> _handle;

    VkExtent2D _extent{};
    VkSurfaceFormatKHR _surfaceFormat{};
    VkPresentModeKHR _presentMode = VK_PRESENT_MODE_FIFO_KHR;

public:
    Swapchain(Swapchain const& other) = delete;
    Swapchain(Swapchain&& other) noexcept = default;
    Swapchain& operator=(Swapchain const& other) = delete;
    Swapchain& operator=(Swapchain&& other) noexcept = default;

    static constexpr void debug_check(Swapchain const&);
};
}

namespace cvk {
struct Swapchain::State {
    vk_not_null<VkSwapchainKHR> vkSwapchain;
    VkExtent2D extent;
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkSwapchainKHR> decompose() && {
        return {vkSwapchain.get()};
    }
};
}

namespace cvk {
constexpr VkExtent2D Swapchain::evalExtent(
    VkExtent2D extent,
    VkSurfaceCapabilitiesKHR const& capabilities
) {
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    return VkExtent2D{
        std::clamp(
            extent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width
        ),
        std::clamp(
            extent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height
        ),
    };
}

constexpr uint32_t Swapchain::evalImageCount(
    uint32_t minImageCount,
    VkSurfaceCapabilitiesKHR const& capabilities
) {
    uint32_t const count = std::max(
        minImageCount != 0 ? minImageCount : capabilities.minImageCount + 1,
        capabilities.minImageCount
    );
    return capabilities.maxImageCount != 0 ? std::min(count, capabilities.maxImageCount) : count;
}

constexpr void Swapchain::debug_check(Swapchain const& swapchain) {
    CTH_CRITICAL(!swapchain.created(), "swapchain must be created") {}
}
}
