#pragma once
#include "jvk/base/queue.hpp"
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/graph/swapchain.hpp"
#include "jvk/sync/resource_tracker.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/render/sync/semaphore.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/coro/catching_scope.hpp>
#include <cth/coro/counter_event.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/move_ptr.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
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
class scoped_acquire;
}

namespace jvk {
/**
 * acquired swapchain image
 * @details
 * - tracked like every image: the commands that use it record its accesses
 * - valid until the end of its scope, see `scoped_acquire`
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
     * @details `cvk::SwapchainConfig::minImageCount` `0` -> `minImageCount` of the surface + `framesInFlight`: an
     *  image for each frame in flight beside the images of the presentation engine
     * @pre `cvk::SwapchainConfig::queueFamilies.size() < 2`: the images stay `VK_SHARING_MODE_EXCLUSIVE`
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
 * cvk surface with its swapchain, acquires and presents its images
 * @details
 * - owns the cvk surface and the swapchain
 * - recreates the swapchain in `acquire()`, a retired swapchain stays until its presents ended, see `Swapchain`
 * - submits and presents in the strand of its queue, acquires outside the strand on `Queue::scheduler()`
 * - `Surface::_presented` orders the uses of the swapchain: a present job sets it at its end, the
 *   next `acquire()` waits for it
 * - one acquired image at a time, in a scope: `scoped_acquire` presents it at its end
 * - an image without a present: the next `acquire()` or `destroy()` unacquires it, `acquire()` after the gpu finished
 *   its stage
 * - the present jobs run detached, `destroy()` waits for them
 * @attention not thread safe
 */
class Surface {
public:
    using Config = SurfaceConfig;

    /**
     * base constructor
     * @details the queue presents and runs the present submits
     * @pre the config passes `Config::debug_check()`
     */
    Surface(Core const&, Queue const&, Config);

    /**
     * constructs and creates
     * @details calls @ref Surface(Core const&, Queue const&, Config), `create()`
     */
    Surface(Core const&, Queue const&, Config, cvk::Surface);

    /**
     * @note calls `optDestroy()`
     */
    ~Surface() { optDestroy(); }

    /**
     * takes the surface, creates the swapchain, the semaphores and the command buffers
     * @details `cvk::Surface::extent()` `0` -> the first `acquire()` with an extent `> 0` creates the swapchain
     * @details a failure destroys the parts and the surface
     * @pre `Core::created()`
     * @pre `cvk::Surface::created()`
     * @pre the queue can present to the surface
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of a create of the swapchain, semaphores or command buffers
     */
    void create(cvk::Surface);

    /**
     * waits for the present jobs, destroys and forgets
     * @details
     * - waits for the present jobs, then until the gpu finished the present submits
     * - then unacquires an image without a present, e.g. after an exception: the earlier frames that used it ended
     * - destroys the command buffers, the semaphores, the swapchain, then the surface
     * - a failed wait does not stop the destruction
     * @pre `created()`
     * @pre no `acquire()` is pending
     * @pre every `scoped_acquire` ended
     * @pre the graph of the presented stages and of the stage of an image without a present lives: the present jobs
     *  wait for its semaphores
     * @pre a stage that waits for an acquire without a present: finished on the gpu, or unsubmitted until
     *  `~StageGraph()`
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
     * acquires the next swapchain image for the stage
     * @details
     * - adds the wait for the acquire to the stage: `Stage::await()` with the acquire semaphore and the stages
     * - the state of the image in the graph of the stage: `VK_IMAGE_LAYOUT_UNDEFINED` with the wait stages, see
     *   `StageGraph::assume()`: the first barrier of the image chains with the wait
     * - waits until the last present job ended: the acquire and the present share the
     *   swapchain, a blocking acquire before that present delays it by one refresh (FIFO)
     * - runs on `Queue::scheduler()`, outside the strand
     * - an out of date acquire of the current swapchain, an out of date present, or a changed
     *   `cvk::Surface::extent()` -> recreates the swapchain first, erases the old images in the
     *   graph of the stage, see `StageGraph::erase()`, `Swapchain::recreate()`
     * - an acquired image without a present, e.g. after an exception or a skip: waits until the gpu finished the
     *   submit or the skip of its stage, then unacquires it
     * @param stages pipeline stages of the stage that wait for the acquire
     * @return scope of the image, see `scoped_acquire`, no image -> surface extent `0`, no wait
     * @pre `created()`
     * @pre `Stage::status()` is `UNSUBMITTED`
     * @pre the `scoped_acquire` of the last acquire ended
     * @pre an acquired image without a present of this frame: its stage is submitted or skipped
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception other result of `vkAcquireNextImageKHR()` or `vkCreateSwapchainKHR()`
     * @throws std::exception the first error of a present job or a scope, stored before the call:
     *  an error of the last present job during the waits -> the next `acquire()` rethrows it, see
     *  `cth::co::catching_scope`
     * @attention `vkAcquireNextImageKHR()` can block (FIFO: the vsync throttle): it occupies a driver thread,
     *  `CoreConfig::driverThreads` `0` -> the calling worker
     * @see `Stage::await()`
     */
    [[nodiscard]] cth::co::executor_task<scoped_acquire> acquire(
        Stage&,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    );

    /**
     * presents the acquired image after the submitted stage
     * @details
     * - returns at once, a job presents after the submit of the stage reached `vkQueueSubmit2()`
     * - transitions the image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` from its last state in the stage primary
     * - a stage of another queue family: the job releases the image on the stage queue, the images stay
     *   `VK_SHARING_MODE_EXCLUSIVE`
     * - `VK_ERROR_OUT_OF_DATE_KHR` -> the next `acquire()` recreates the swapchain
     * - a failure of the job: the next `acquire()` rethrows it
     * @pre an acquired image without a present
     * @pre `Stage::status()` is `SUBMITTED`
     * @pre the stage recording uses the image
     * @pre the stage is the last use of the image in the frame or waits for the other uses
     * @see `scoped_acquire`
     */
    void present(Stage&);

    [[nodiscard]] constexpr bool created() const { return _surface.created(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

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
     * submit before the present of one swapchain image
     * @details signals the present semaphore of the image
     * @see `cvk::Swapchain::presentSemaphore()`
     */
    struct PresentSubmit {
        PrimaryCmdBuffer cmd; ///< the barrier of the image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`

        /**
         * value of `Surface::_semaphore` for the last submit of `PresentSubmit::cmd`, free once reached
         */
        uint64_t value = 0;
    };

    /**
     * acquired image without a present
     * @details its stage waits for the acquire semaphore on the gpu: the unacquire waits until the gpu finished the
     *  submit or the skip of the stage
     */
    struct AcquiredImage {
        SwapchainImage image;
        /** `Stage::semaphore()` of the stage of the acquire */
        cth::ref_wrapper<Semaphore const> stageSemaphore;
        uint64_t frameNumber; ///< `Stage::frameNumber()`, the value of `stageSemaphore` to wait for
    };

    // no awaiter parameter: the job takes `submitted()` from the stage semaphore and the frame
    // number
    // the image as a parameter: the job reads no surface state outside of the strand
    // `_presented` of the job before awaited: acquire() no longer orders the present jobs
    /**
     * present job of `present()`
     * @details the order:
     * - off the strand: awaits `Semaphore::submitted()` of the stage semaphore with the frame number
     * - off the strand: awaits `Surface::_presented` of the job before: the present jobs run one
     *   at a time, in the order of `present()`
     * - fills `Surface::_submitInfo`: the job before ended its use
     * - stage of another family -> `Queue::transfer()` of the release half on the stage queue
     * - off the strand: records the command buffer, the present jobs run one at a time
     * - `Queue::present()` with the current swapchain: a recreation only happens in `acquire()`,
     *   after the last present
     * - the submit waits for the semaphore of the stage and the release, signals
     *   `Surface::_semaphore` with the value, `Queue::present()` marks it after the present
     * - the command buffer: the acquire half of another family, the transition to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`
     * - `VK_ERROR_OUT_OF_DATE_KHR` of the present: sets `Surface::_outOfDate` before
     *   `Surface::_presented`
     * - the last step, also after a failure: sets `Surface::_presented` to the value
     * @details the errors:
     * - a failed submit: stores the error, then `dev::skip_submit()` that signals only
     *   `Surface::_semaphore`, no present
     * - a failed skip submit: `VK_ERROR_DEVICE_LOST` -> stores the error, marks, else terminates:
     *   the waiters would hang
     * - a failed present: stores the error
     * - before the end: every other failure terminates, the waiters on `Surface::_presented`
     *   would hang
     * @param image the acquired image
     * @param lastState of the image in the stage primary
     * @param value of `Surface::_semaphore` for the submit
     * @pre the command buffer of the submit belongs to the queue family of the queue
     */
    [[nodiscard]] cth::co::executor_task<void> presentJob(
        Queue const& stageQueue,
        Semaphore const& stageSemaphore,
        uint64_t frameNumber,
        SwapchainImage image,
        ImageState lastState,
        uint64_t value
    );

    /**
     * creates or recreates the swapchain if it is not created, out of date or of another extent
     * @details
     * - the first creation: the config of `swapchainConfig()`
     * - a recreation erases the old images in the graph first: the new images can reuse their handles
     * - grows `Surface::_submits` to the image count
     * @param extent of the surface, `> 0`
     * @param graphPtr of the acquiring stage, `nullptr` -> no graph
     * @pre no present job uses the swapchain: `create()`, or `acquire()` after the end of the last
     *  present job
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of `vkCreateSwapchainKHR()`
     */
    void updateSwapchain(VkExtent2D extent, StageGraph* graphPtr);

    /**
     * @return `SurfaceConfig::swapchain`, `minImageCount` `0` -> `minImageCount` of the surface + `framesInFlight`
     */
    [[nodiscard]] Swapchain::Config swapchainConfig() const;

    /**
     * gives the acquired image back to the current swapchain
     * @pre an acquired image of the current swapchain
     * @pre no present job uses the swapchain: `acquire()` after the end of the last present job, or
     *  `destroy()`
     * @throws cvk::vk_result_exception result of `vkReleaseSwapchainImagesKHR()`
     */
    void unacquireImage();

    cth::ref_wrapper<Core const> _core;
    cth::ref_wrapper<Queue const> _queue;
    Config _config;

    // destroyed after the swapchain
    cvk::Surface _surface;

    /** created once the surface extent is `> 0`, `Surface::_presented` orders its uses */
    Swapchain _swapchain;

    /**
     * ring of `SurfaceConfig::framesInFlight`, signaled by the acquires
     */
    std::vector<cvk::Semaphore> _acquireSemaphores{};

    /** the semaphore in `Surface::_acquireSemaphores` for the next acquire */
    size_t _acquireSemaphoreIdx = 0;

    Semaphore _semaphore; ///< signaled by the present submits, each submit signals the next value

    /** value of the last ended present job, `acquire()` and the next present job wait for it */
    cth::co::counter_event _presented{};

    std::vector<PresentSubmit> _submits{}; ///< one per swapchain image

    /**
     * the submit of the present job that runs, `Surface::_presented` orders its uses
     * @details each present job fills it: it keeps its capacity for the next jobs
     */
    SubmitInfo _submitInfo{};

    /** the acquired image without a present */
    std::optional<AcquiredImage> _acquiredOpt = std::nullopt;

    /**
     * set by an out of date acquire and by the present jobs
     * @details `acquire()` reads it after `Surface::_presented` reached the last present
     */
    bool _outOfDate = false;

    /**
     * the running present jobs, `destroy()` waits for them
     * @details the first error of a job or a scope, `acquire()` rethrows it
     */
    cth::co::catching_scope _jobs{};

public:
    Surface(Surface const& other) = delete;
    Surface(Surface&& other) = delete;
    Surface& operator=(Surface const& other) = delete;
    Surface& operator=(Surface&& other) = delete;

    static constexpr void debug_check(Surface const&);
};
}

namespace jvk {
/**
 * acquired swapchain image of a stage for the scope, presents it at its end
 * @details
 * - `Surface::acquire()` returns it
 * - end, by `Stage::status()`: `SUBMITTED` -> `Surface::present()`, also after an exception: the frame is complete
 * - end, by `Stage::status()`: `SKIPPED` or `UNSUBMITTED` -> nothing, the next `Surface::acquire()` or
 *   `Surface::destroy()` unacquires the image
 * - no image or a moved-from scope: nothing
 * - a failure at the end, e.g. an allocation: the next `Surface::acquire()` rethrows it
 * - `->` and `*` reach the image
 * - declare it before the `scoped_submit` of the stage: the `scoped_submit` ends first and submits
 * @attention a coroutine destroyed while suspended with the scope alive presents the image of a submitted stage
 */
class scoped_acquire {
public:
    /**
     * @param surface of the image
     * @param stage of the acquire
     * @param imageOpt `std::nullopt` -> no image
     * @param jobs of the surface, gets a failure at the end
     */
    scoped_acquire(
        Surface& surface,
        Stage& stage,
        std::optional<SwapchainImage> imageOpt,
        cth::co::catching_scope& jobs
    ) noexcept;

    ~scoped_acquire();

    /**
     * presents the image now, the end of the scope does nothing
     * @details `Stage::status()` is `UNSUBMITTED` -> calls `Stage::submit()` first
     * @pre an image
     * @pre `Stage::status()` is not `SKIPPED`
     * @pre the recording of the stage uses the image
     * @see `Surface::present()`
     */
    void present();

    /**
     * ends the scope without a present, the end of the scope does nothing
     * @details the next `Surface::acquire()` or `Surface::destroy()` unacquires the image
     * @pre an image
     * @see `Surface::acquire()`
     */
    void unacquire();

    [[nodiscard]] constexpr explicit operator bool() const { return _imageOpt.has_value(); }

    /**
     * @pre an image
     */
    [[nodiscard]] constexpr SwapchainImage const* operator->() const { return &*_imageOpt; }

    /**
     * @pre an image
     */
    [[nodiscard]] constexpr SwapchainImage const& operator*() const { return *_imageOpt; }

private:
    cth::move_ptr<Surface> _surfacePtr; ///< `nullptr` -> the end does nothing
    cth::ref_wrapper<Stage> _stage;
    std::optional<SwapchainImage> _imageOpt;
    cth::ref_wrapper<cth::co::catching_scope> _jobs; ///< of the surface

public:
    scoped_acquire(scoped_acquire const& other) = delete;
    scoped_acquire(scoped_acquire&& other) noexcept = default;
    scoped_acquire& operator=(scoped_acquire const& other) = delete;
    scoped_acquire& operator=(scoped_acquire&& other) = delete;

    static constexpr void debug_check(scoped_acquire const&);
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

constexpr void scoped_acquire::debug_check(scoped_acquire const& image) {
    CTH_CRITICAL(
        !image._surfacePtr,
        "the scope must hold the image, got a moved-from or ended scope"
    ) {}
    CTH_CRITICAL(!image._imageOpt.has_value(), "the scope must hold an image, got none") {}
}
}
