#pragma once
#include "jvk/base/queue.hpp"
#include "jvk/graph/pending_present.hpp"
#include "jvk/graph/swapchain.hpp"

#include "cvk/render/sync/semaphore.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>


namespace cvk {
class Image;
class ImageView;
}

namespace jvk {
class Core;
class Stage;
class StageGraph;
}

namespace jvk {
/**
 * acquired swapchain image
 * @details
 * - tracked like every image: the commands that use it record its accesses
 * - valid until the present stage of its acquire presented or dropped it, see
 *   `Surface::acquire()`
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
     * frames that the gpu works on at the same time, one acquire semaphore each
     * @pre `framesInFlight > 0`
     * @pre `framesInFlight >= StageGraphConfig::frameSlots` of the graph of the stages
     */
    uint32_t framesInFlight = 2;

    static constexpr void debug_check(SurfaceConfig const&);
};

/**
 * cvk surface with its swapchain, acquires its images for stages that present them
 * @details
 * - owns the cvk surface and the swapchain
 * - recreates the swapchain in `acquire()`, a retired swapchain stays until its presents ended, see
 *   `Swapchain`
 * - acquires outside the strand of its queue on `Queue::scheduler()`
 * - the present stage of an image presents it after its submit, see `acquire()`: the surface has
 *   no jobs, the errors of the presents go to the graph of the present stage
 * - one acquired image at a time: `acquire()` waits until the last image is presented or dropped
 * - a dropped image: the next `acquire()` or `destroy()` unacquires it after the gpu finished its
 *   stages
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
     * takes the cvk surface, creates the swapchain and the acquire semaphores
     * @details `cvk::Surface::extent()` `0` -> the first `acquire()` with an extent `> 0` creates
     *  the swapchain
     * @details a failure destroys the parts and the cvk surface
     * @pre `Core::created()`
     * @pre `cvk::Surface::created()`
     * @pre the queue can present to the cvk surface
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of a create of the swapchain or the semaphores
     */
    void create(cvk::Surface);

    /**
     * waits for the last image, destroys and forgets
     * @details
     * - blocks until the last acquired image is presented or dropped
     * - a dropped last image: blocks until the gpu finished its stages, then unacquires it
     * - destroys the acquire semaphores, the swapchain, then the cvk surface
     * - a failed wait does not stop the destruction
     * @pre `created()`
     * @pre no `acquire()` is pending
     * @pre the present stage of the last image is submitted or skipped, or its graph is destroyed:
     *  else the wait never ends, e.g. destroy the graphs of the stages first
     * @pre the submits that wait for the acquire semaphores finished on the gpu
     * @attention blocks the calling thread: a worker of the core must be free for the jobs of the
     *  graph
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
     * acquires the next swapchain image, the present stage presents it after its submit
     * @details the acquire stage:
     * - waits for the acquire: `Stage::await()` with the acquire semaphore and the stages
     * - the state of the image in its graph: `VK_IMAGE_LAYOUT_UNDEFINED` with the wait stages, see
     *   `StageGraph::assume()`: the first barrier of the image chains with the wait
     * @details the present stage gets the present of the image, see `Stage::present()`:
     * - its submit ends with the barrier of the image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`, from
     *   the state that the stages before left: the epilogue, after the primary
     * - another queue family than the queue of the surface: the epilogue releases the image, the
     *   queue of the surface acquires it, see `Queue::transfer()`
     * - after its submit: the present on the queue of the surface, one `vkQueuePresentKHR()` for
     *   the images of one queue
     * - a failed present: the graph stores the error, `StageGraph::nextFrame()` rethrows it
     * - a skip or a failed submit drops the image: the next `acquire()` unacquires it
     * @details the surface:
     * - waits until the last image is presented or dropped
     * - a dropped last image: waits until the gpu finished its stages, then unacquires it
     * - an out of date acquire or present, or a changed `cvk::Surface::extent()` -> recreates the
     *   swapchain first, erases the old images in the graph, see `StageGraph::erase()`,
     *   `Swapchain::recreate()`
     * - runs on `Queue::scheduler()`, outside the strand
     * @param acquireStage waits for the acquire, the first use of the image
     * @param presentStage presents the image after its submit
     * @param stages pipeline stages of the acquire stage that wait for the acquire
     * @return the image, `std::nullopt` -> surface extent `0`: no image, no wait, no present
     * @pre `created()`
     * @pre `Stage::status()` of both stages is `UNSUBMITTED`
     * @pre both stages belong to one frame of one graph
     * @pre the present stage is the acquire stage or waits for it in the frame, directly or through
     *  other stages, see `StageWait`
     * @pre the present stage of the last image of this surface is submitted or skipped: else the
     *  wait never ends
     * @pre the last use of the image before the present is on the queue family of the present stage
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception other result of `vkAcquireNextImageKHR()` or
     *  `vkCreateSwapchainKHR()`
     * @attention `vkAcquireNextImageKHR()` can block (FIFO: the vsync throttle): it occupies a
     *  driver thread, `CoreConfig::driverThreads` `0` -> the calling worker
     * @see `Stage::await()`
     */
    [[nodiscard]] cth::co::executor_task<std::optional<SwapchainImage>> acquire(
        Stage& acquireStage,
        Stage& presentStage,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    );

    /**
     * acquires the next swapchain image for one stage that uses and presents it
     * @details calls @ref acquire(Stage&, Stage&, VkPipelineStageFlags2) with the stage for both
     */
    [[nodiscard]] cth::co::executor_task<std::optional<SwapchainImage>> acquire(
        Stage& stage,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    ) {
        return acquire(stage, stage, stages);
    }

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
    // a recreation erases the old images in the graph first: the new images can reuse their
    // handles; only when no present uses the swapchain
    void updateSwapchain(VkExtent2D extent, StageGraph* graphPtr);

    [[nodiscard]] Swapchain::Config swapchainConfig() const;

    cth::ref_wrapper<Core const> _core;
    cth::ref_wrapper<Queue> _queue;
    Config _config;

    // destroyed after the swapchain
    cvk::Surface _surface;

    // the resolve of the last image orders its uses
    Swapchain _swapchain;

    std::vector<cvk::Semaphore> _acquireSemaphores{};

    size_t _acquireSemaphoreIdx = 0;

    dev::PresentState _presentState{};

    // `0` -> none
    uint64_t _lastValue = 0;

    uint32_t _lastImageIdx = 0;

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
