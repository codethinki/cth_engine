#pragma once
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/graph/swapchain.hpp"
#include "jvk/sync/resource_states.hpp"
#include "jvk/sync/semaphore.hpp"
#include "jvk/utility/first_error.hpp"

#include "cvk/render/sync/semaphore.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/coro/simple_counting_scope.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/move_ptr.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <optional>
#include <vector>


namespace cvk {
class Image;
class ImageView;
}

namespace jvk {
class Core;
class Queue;
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
 * config of a `Presenter`
 */
struct PresenterConfig {
    /**
     * @details `cvk::SwapchainConfig::minImageCount` `0` -> `minImageCount` of the surface + `framesInFlight`: the
     *  limit of the acquired images for an acquire without a timeout
     * @pre `cvk::SwapchainConfig::queueFamilies.size() < 2`: the images stay `VK_SHARING_MODE_EXCLUSIVE`
     * @pre `cvk::SwapchainConfig::minImageCount != 0` -> `>= minImageCount` of the surface + `framesInFlight`
     */
    cvk::Swapchain::Config swapchain{};

    /**
     * frames that the gpu works on at the same time, one acquire semaphore each
     * @pre `framesInFlight > 0`
     * @pre `framesInFlight >= StageGraphConfig::frameSlots` of the graph of the stages
     */
    uint32_t framesInFlight = 2;

    static constexpr void debug_check(PresenterConfig const&);
};

/**
 * presents to a surface
 * @details
 * - owns the surface and the swapchain
 * - recreates the swapchain in `acquire()`, a retired swapchain presents its pending images, see `Swapchain`
 * - acquires, submits and presents in the strand of its queue
 * - one acquired image at a time, in a scope: `scoped_acquire` presents or unacquires it at its end
 * - the present and unacquire jobs run detached, `destroy()` waits for them
 * @attention not thread safe
 */
class Presenter {
public:
    using Config = PresenterConfig;

    /**
     * base constructor
     * @details the queue presents and runs the present submits
     * @pre the config passes `Config::debug_check()`
     */
    Presenter(Core const&, Queue const&, Config);

    /**
     * constructs and creates
     * @details calls @ref Presenter(Core const&, Queue const&, Config), `create()`
     */
    Presenter(Core const&, Queue const&, Config, cvk::Surface);

    /**
     * @note calls `optDestroy()`
     */
    ~Presenter() { optDestroy(); }

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
     * - waits for the present and unacquire jobs, then unacquires an image without a present, e.g. after an exception
     * - waits until the gpu finished the present submits
     * - destroys the command buffers, the semaphores, the swapchain, then the surface
     * - a failed wait does not stop the destruction
     * @pre `created()`
     * @pre no `acquire()` is pending
     * @pre every `scoped_acquire` ended
     * @pre the graph of the presented stages lives: the present jobs wait for its semaphores
     * @pre a stage that waits for an acquire without a present: finished on the gpu, or unsubmitted until
     *  `~StageGraph()`
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * acquires the next swapchain image for the stage
     * @details
     * - adds the wait for the acquire to the stage: `Stage::await()` with the acquire semaphore and the stages
     * - the state of the image in the graph of the stage: `VK_IMAGE_LAYOUT_UNDEFINED` with the wait stages, see
     *   `StageGraph::assume()`: the first barrier of the image chains with the wait
     * - waits until at most `PresenterConfig::framesInFlight - 1` present jobs are pending: the limit of the
     *   acquired images, see `PresenterConfig::swapchain`
     * - `VK_ERROR_OUT_OF_DATE_KHR` of a present or an acquire of the current swapchain, or a changed
     *   `cvk::Surface::extent()` -> recreates the swapchain first, erases the old images in the graph of the stage,
     *   see `StageGraph::erase()`, `Swapchain::recreate()`
     * - all images in use -> awaits the oldest present fence first
     * - an acquired image without a present, e.g. after an exception: unacquires it
     * @param stages pipeline stages of the stage that wait for the acquire
     * @return scope of the image, see `scoped_acquire`, no image -> surface extent `0`, no wait
     * @pre `created()`
     * @pre `Stage::status()` is `UNSUBMITTED`
     * @pre the `scoped_acquire` of the last acquire ended, presented or unacquired
     * @pre an acquired image without a present: no submit uses it
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception other result of `vkAcquireNextImageKHR()` or `vkCreateSwapchainKHR()`
     * @throws cvk::vk_result_exception the first error of a present job or a scope
     * @attention the strand of the queue can block in `vkAcquireNextImageKHR()` while no image is free: the
     *  presentation engine can hold an image after its present fence
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
     * @pre an acquired image without a present or unacquire
     * @pre `Stage::status()` is `SUBMITTED`
     * @pre the stage recording uses the image
     * @pre the stage is the last use of the image in the frame or waits for the other uses
     * @see `scoped_acquire`
     */
    void present(Stage&);

    /**
     * gives the acquired image back without a present
     * @details returns at once: the strand of the queue gives it back before the next `acquire()`
     * @pre an acquired image, not presented
     * @pre no submit uses the image
     */
    void unacquire();

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
         * value of `Presenter::_semaphore` for the last submit of `PresentSubmit::cmd`, free once reached
         */
        uint64_t value = 0;
    };

    //TODO review no submitted_awaiter parameter: the job takes it from the stage semaphore and the frame number
    //TODO review the image and its swapchain as parameters: the job reads no presenter state outside of the strand
    //TODO review the submit mark of the present job before awaited: acquire() no longer orders the present jobs
    /**
     * present job of `present()`
     * @details the order:
     * - off the strand: awaits `Semaphore::submitted()` of the stage semaphore with the frame number
     * - stage of another family -> `Queue::release()` of the image on the stage queue
     * - off the strand: awaits `Semaphore::submitted()` of `Presenter::_semaphore` with the value before: the present
     *   jobs submit and present in the order of `present()`
     * - in the strand of the queue: records the command buffer, submits it through `Queue::vk()`, presents through
     *   the swapchain of the image, also a retired one
     * - the submit waits for the semaphore of the stage and the release, signals the present semaphore and
     *   `Presenter::_semaphore` with the value
     * - the command buffer: the acquire half of another family, the transition to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`
     * - `Semaphore::markSubmitted()` of `Presenter::_semaphore` with the value, after the present
     * @details the errors:
     * - a failed submit: stores the error, then an empty submit that signals only `Presenter::_semaphore`, no present
     * - a failed empty submit: `VK_ERROR_DEVICE_LOST` -> stores the error, else terminates: the waiters would hang
     * - a failed present: stores the error
     * @param image the acquired image
     * @param swapchain of the image, current at `present()`
     * @param lastState of the image in the stage primary
     * @param value of `Presenter::_semaphore` for the submit
     * @pre the command buffer of the submit belongs to the queue family of the queue
     */
    [[nodiscard]] cth::co::executor_task<void> presentJob(
        Queue const& stageQueue,
        Semaphore const& stageSemaphore,
        uint64_t frameNumber,
        SwapchainImage image,
        cvk::Swapchain& swapchain,
        ImageState lastState,
        uint64_t value
    );

    /**
     * awaits the present fence of the image of the current swapchain without a block of the strand
     * @details exports `cvk::Swapchain::presentFence()`, awaits it with `cth::co::this_coro::await_handle()`
     * @pre runs in the strand of the queue
     * @throws cvk::vk_result_exception result of the export, see `cvk::Fence::extractOsHandle()`
     */
    [[nodiscard]] cth::co::executor_task<void> awaitPresentFence(uint32_t imageIdx) const;

    /**
     * creates or recreates the swapchain if it is not created, out of date or of another extent
     * @details
     * - the first creation: the config of `swapchainConfig()`
     * - a recreation erases the old images in the graph first: the new images can reuse their handles
     * - a recreation forgets the pending presents of `Presenter::_presentedImageIndices`, they are of the old swapchain
     * - grows `Presenter::_submits` to the image count
     * @param extent of the surface, `> 0`
     * @param graphPtr of the acquiring stage, `nullptr` -> no graph
     * @pre runs in the strand of the queue
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of `vkCreateSwapchainKHR()`
     */
    void updateSwapchain(VkExtent2D extent, StageGraph* graphPtr);

    /**
     * @return `PresenterConfig::swapchain`, `minImageCount` `0` -> `minImageCount` of the surface + `framesInFlight`
     */
    [[nodiscard]] Swapchain::Config swapchainConfig() const;

    /**
     * gives the acquired image back to the current swapchain
     * @pre an acquired image of the current swapchain
     * @pre runs in the strand of the queue or no job uses the swapchain
     * @throws cvk::vk_result_exception result of `vkReleaseSwapchainImagesKHR()`
     */
    void unacquireImage();

    cth::ref_wrapper<Core const> _core;
    cth::ref_wrapper<Queue const> _queue;
    Config _config;

    // destroyed after the swapchain
    cvk::Surface _surface;

    Swapchain _swapchain; ///< created once the surface extent is `> 0`, used in the strand of the queue

    /**
     * ring of `PresenterConfig::framesInFlight`, signaled by the acquires
     */
    std::vector<cvk::Semaphore> _acquireSemaphores{};

    size_t _acquireSemaphoreIdx = 0; ///< the semaphore in `Presenter::_acquireSemaphores` for the next acquire

    Semaphore _semaphore; ///< signaled by the present submits, each submit signals the next value

    std::vector<PresentSubmit> _submits{}; ///< one per swapchain image

    /**
     * the images of the pending presents of the current swapchain, the oldest first
     */
    std::deque<uint32_t> _presentedImageIndices{};

    std::optional<SwapchainImage> _imageOpt = std::nullopt; ///< the acquired image, not presented or unacquired

    /**
     * `VK_ERROR_OUT_OF_DATE_KHR` of a present of the current swapchain, the next `acquire()` recreates the swapchain
     */
    std::atomic<bool> _outOfDate = false;

    dev::first_error _error{}; ///< the first error of a job or a scope, `acquire()` rethrows it

    cth::co::simple_counting_scope _jobs{}; ///< the running present and unacquire jobs, `destroy()` waits for them

public:
    Presenter(Presenter const& other) = delete;
    Presenter(Presenter&& other) = delete;
    Presenter& operator=(Presenter const& other) = delete;
    Presenter& operator=(Presenter&& other) = delete;

    static constexpr void debug_check(Presenter const&);
};
}

namespace jvk {
/**
 * acquired swapchain image of a stage for the scope, presents or unacquires it at its end
 * @details
 * - `Presenter::acquire()` returns it
 * - normal end, by `Stage::status()`: `SUBMITTED` -> `Presenter::present()`, `SKIPPED` or `UNSUBMITTED` ->
 *   `Presenter::unacquire()`
 * - exception, by `Stage::status()`: `SUBMITTED` -> presents, the frame is complete, else nothing: the next
 *   `Presenter::acquire()` or `Presenter::destroy()` unacquires the image
 * - no image or a moved-from scope: nothing
 * - a failure at the end, e.g. an allocation: the next `Presenter::acquire()` rethrows it
 * - `->` and `*` reach the image
 * - declare it before the `scoped_submit` of the stage: the `scoped_submit` ends first and submits
 * @attention a coroutine destroyed while suspended with the scope alive presents or unacquires the image
 */
class scoped_acquire {
public:
    //TODO review the uncaught exceptions as a parameter: acquire() reads them on the calling thread before it moves
    // onto the strand, the scope is constructed in the strand
    /**
     * @param presenter of the image
     * @param stage of the acquire
     * @param imageOpt `std::nullopt` -> no image
     * @param error the first error of the presenter, gets a failure at the end
     * @param uncaughtExceptions of the calling thread at the start of `Presenter::acquire()`, tell the exception at
     *  the end
     */
    scoped_acquire(
        Presenter& presenter,
        Stage& stage,
        std::optional<SwapchainImage> imageOpt,
        dev::first_error& error,
        int uncaughtExceptions
    ) noexcept;

    ~scoped_acquire();

    /**
     * presents the image now, the end of the scope does nothing
     * @details `Stage::status()` is `UNSUBMITTED` -> calls `Stage::submit()` first
     * @pre an image
     * @pre `Stage::status()` is not `SKIPPED`
     * @pre the recording of the stage uses the image
     * @see `Presenter::present()`
     */
    void present();

    /**
     * gives the image back without a present, the end of the scope does nothing
     * @pre an image
     * @pre no submit uses the image
     * @see `Presenter::unacquire()`
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
    cth::move_ptr<Presenter> _presenterPtr; ///< `nullptr` -> the end does nothing
    cth::ref_wrapper<Stage> _stage;
    std::optional<SwapchainImage> _imageOpt;
    cth::ref_wrapper<dev::first_error> _error; ///< the first error of the presenter
    int _uncaughtExceptions; ///< of the calling thread at the start of `Presenter::acquire()`

public:
    scoped_acquire(scoped_acquire const& other) = delete;
    scoped_acquire(scoped_acquire&& other) noexcept = default;
    scoped_acquire& operator=(scoped_acquire const& other) = delete;
    scoped_acquire& operator=(scoped_acquire&& other) = delete;

    static constexpr void debug_check(scoped_acquire const&);
};
}

namespace jvk {
constexpr void PresenterConfig::debug_check(PresenterConfig const& config) {
    CTH_CRITICAL(config.framesInFlight == 0, "framesInFlight must be > 0") {}
    CTH_CRITICAL(
        config.swapchain.queueFamilies.size() >= 2,
        "the swapchain images must stay exclusive, got {} queue families",
        config.swapchain.queueFamilies.size()
    ) {}
}

constexpr void Presenter::debug_check(Presenter const& presenter) {
    CTH_CRITICAL(!presenter.created(), "presenter must be created") {}
}

constexpr void scoped_acquire::debug_check(scoped_acquire const& image) {
    CTH_CRITICAL(!image._presenterPtr, "the scope must hold the image, got a moved-from or ended scope") {}
    CTH_CRITICAL(!image._imageOpt.has_value(), "the scope must hold an image, got none") {}
}
}
