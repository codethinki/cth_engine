#pragma once
#include "jvk/base/queue.hpp"
#include "jvk/graph/presenter.hpp"
#include "jvk/graph/swapchain.hpp"

#include "cvk/surface/surface.hpp"
#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstdint>
#include <optional>


namespace cvk {
class Image;
class ImageView;
}

namespace jvk {
class Core;
}

namespace jvk {
/**
 * acquired swapchain image
 * @details
 * - tracked like every image: the commands that use it record its accesses
 * - a view of the `Presenter::AcquiredImage` that the present stage holds
 * - valid until the present stage of its acquire presented or dropped it, see
 *   `Frame::acquire()`
 */
struct SwapchainImage {
    cth::ref_wrapper<cvk::Image const> image;
    cth::ref_wrapper<cvk::ImageView const> view;

    uint32_t imageIdx; ///< index of the image in the swapchain

    VkExtent2D extent; ///< extent of the swapchain at the acquire
};

/**
 * config of a `Surface`
 */
struct SurfaceConfig {
    /**
     * @details `cvk::SwapchainConfig::minImageCount` `0` -> `minImageCount` of the surface +
     *  `framesInFlight`: an image for each frame in flight beside the images of the presentation
     *  engine
     * @pre `cvk::SwapchainConfig::queueFamilies.size() < 2`: the images stay
     *  `VK_SHARING_MODE_EXCLUSIVE`
     */
    cvk::Swapchain::Config swapchain{};

    /**
     * frames that the gpu works on at the same time, the acquire semaphores of each presenter
     * @details `< StageGraphConfig::frameSlots` of the graph of the images -> an acquire can wait
     *  for the present of an earlier frame, see `Presenter::acquire()`
     * @pre `framesInFlight > 0`
     */
    uint32_t framesInFlight = 2;

    static constexpr void debug_check(SurfaceConfig const&);
};

/**
 * cvk surface with its swapchain, acquires the images of the current swapchain
 * @details
 * - owns the cvk surface and the swapchain with its presenters, see `Swapchain`
 * - recreates the swapchain in `acquire()`, a retired swapchain stays until its presenter
 *   finished
 * - the presenter of the current swapchain acquires the images, see `Presenter::acquire()`
 * - knows no graph: `Frame::acquire()` hands an image to the stages of a frame
 * - a failed present: the next `acquire()` rethrows its error
 * @attention not thread safe
 */
class Surface {
public:
    using Config = SurfaceConfig;

    /**
     * base constructor
     * @details the queue presents the images
     * @pre the config passes `Config::debug_check()`
     */
    Surface(Core const&, Queue&, Config);

    /**
     * constructs and creates
     * @details calls @ref Surface(Core const&, Queue&, Config), `create()`
     */
    Surface(Core const&, Queue&, Config, cvk::Surface);

    /**
     * @note calls `optDestroy()`
     */
    ~Surface() { optDestroy(); }

    /**
     * takes the cvk surface, creates the swapchain and its presenter
     * @details `cvk::Surface::extent()` `0` -> the first `acquire()` with an extent `> 0` creates
     *  the swapchain
     * @details a failure destroys the parts and the cvk surface
     * @pre `Core::created()`
     * @pre `cvk::Surface::created()`
     * @pre the queue can present to the cvk surface
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of a create of the swapchain or its presenter
     */
    void create(cvk::Surface);

    /**
     * destroys the swapchains with their presenters, then the cvk surface, and forgets
     * @details the presenters release their spares, see `~Presenter()`
     * @pre `created()`
     * @pre no `acquire()` is pending
     * @pre every image of the surface is presented or dropped and the gpu finished its stages:
     *  e.g. `~StageGraph()` of the graph of the stages
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
     * acquires the next image of the current swapchain
     * @details
     * - a new `cvk::Surface::extent()` or out of date -> recreates the swapchain, see
     *   `Swapchain::recreate()`
     * - the presenter of the current swapchain acquires the image, see `Presenter::acquire()`
     * - out of date -> recreates the swapchain and acquires again
     * @return the image, `std::nullopt` -> surface extent `0`: no image
     * @pre `created()`
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception other result of `vkAcquireNextImageKHR()` or
     *  `vkCreateSwapchainKHR()`
     * @throws std::exception the error of a failed present before, once, see `Presenter::acquire()`
     * @attention the fallback of `Presenter::acquire()` can block (FIFO: the vsync throttle): it
     *  occupies a driver thread, `CoreConfig::driverThreads` `0` -> a worker
     * @see `Frame::acquire()`
     */
    [[nodiscard]] cth::co::executor_task<std::optional<Presenter::AcquiredImage>> acquire();

    [[nodiscard]] constexpr bool created() const { return _surface.created(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_queue; }

    /**
     * @details created once the surface extent is `> 0`
     * @pre `created()`
     */
    [[nodiscard]] constexpr Swapchain const& swapchain() const {
        debug_check(*this);
        return _swapchain;
    }

private:
    /**
     * creates the swapchain or recreates it for a new extent or out of date
     */
    void updateSwapchain(VkExtent2D extent, bool outOfDate);

    [[nodiscard]] Swapchain::Config swapchainConfig() const;

    cth::ref_wrapper<Core const> _core;
    cth::ref_wrapper<Queue> _queue;
    Config _config;

    // destroyed after the swapchain
    cvk::Surface _surface;

    Swapchain _swapchain;

public:
    Surface(Surface const& other) = delete;
    Surface(Surface&& other) = delete;
    Surface& operator=(Surface const& other) = delete;
    Surface& operator=(Surface&& other) = delete;

    static constexpr void debug_check(Surface const&);
};
}

namespace jvk {
constexpr void SurfaceConfig::debug_check(SurfaceConfig const& config) {
    CTH_CRITICAL(config.framesInFlight == 0, "framesInFlight must be > 0") {}
    CTH_CRITICAL(
        config.swapchain.queueFamilies.size() >= 2,
        "the swapchain images must stay exclusive, got {} queue families",
        config.swapchain.queueFamilies.size()
    ) {}
}

constexpr void Surface::debug_check(Surface const& surface) {
    CTH_CRITICAL(!surface.created(), "surface must be created") {}
}
}
