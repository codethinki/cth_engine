#pragma once
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/resource_states.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/render/sync/semaphore.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
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
}

namespace jvk {
/**
 * acquired swapchain image
 * @details
 * - tracked like every image: the commands that use it record its accesses
 * - valid until the next `Presenter::acquire()`
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
     * @pre `cvk::SwapchainConfig::queueFamilies.size() < 2`: the images stay `VK_SHARING_MODE_EXCLUSIVE`
     */
    cvk::Swapchain::Config swapchain{};

    /**
     * frames that the gpu works on at the same time, one acquire semaphore each
     * @pre `framesInFlight > 0`
     * @pre `framesInFlight >= StageGraphConfig::frameSlots` of the graph of the stages
     */
    uint32_t framesInFlight = 2;
};

/**
 * presents to a surface
 * @details
 * - owns the surface and the swapchain
 * - recreates the swapchain in `acquire()`
 * - acquires, submits and presents in the strand of its queue
 * - one acquired image at a time
 * @attention not thread safe
 */
class Presenter {
public:
    using Config = PresenterConfig;

    /**
     * base constructor
     * @details the queue presents and runs the present submits
     * @pre `Core::created()`
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

    //TODO cvk::Surface::extent() (2026-11-08): create() and acquire() need the extent of the surface
    /**
     * takes the surface, creates the swapchain, the semaphores and the command buffers
     * @details surface extent `0` -> the first `acquire()` with an extent `> 0` creates the swapchain
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
     * @details destroys the command buffers, the semaphores, the swapchain, then the surface
     * @pre `created()`
     * @pre no `acquire()` is pending
     * @pre the acquired image is presented or unacquired
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() { if(created()) destroy(); }

    //TODO review no alias for the nested result type: the public result stays visible
    /**
     * acquires the next swapchain image for the stage
     * @details
     * - adds the wait for the acquire to the stage
     * - assumes `VK_IMAGE_LAYOUT_UNDEFINED` with the wait stages in the primary of the stage, see
     *   `PrimaryCmdBuffer::assume()`: the first barrier of the image chains with the wait
     * - `VK_ERROR_OUT_OF_DATE_KHR` of the last present or acquire, or a changed surface extent -> recreates the
     *   swapchain first
     * - all images in use -> awaits the oldest present fence first
     * - an acquired image without `present()` or `unacquire()` -> unacquires it, warns
     * @param stages pipeline stages of the stage that wait for the acquire
     * @return `std::nullopt` -> surface extent `0`, no image, no wait
     * @pre `created()`
     * @pre `Stage::status()` is `UNSUBMITTED`
     * @pre `Stage::beginPrimary()` in this frame, not again after the acquire
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception other result of `vkAcquireNextImageKHR()` or `vkCreateSwapchainKHR()`
     * @throws cvk::vk_result_exception the first error of a present job
     * @see `Stage::addWait()`
     */
    [[nodiscard]] cth::co::executor_task<std::optional<SwapchainImage>> acquire(
        Stage&,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    );

    /**
     * presents the acquired image after the stage
     * @details by `Stage::status()`:
     * - `UNSUBMITTED`: submits the stage, then presents
     * - `SUBMITTED`: presents
     * - `SKIPPED`: calls `unacquire()`
     * @details by queue family of the stage, the images stay `VK_SHARING_MODE_EXCLUSIVE`:
     * - another family: the present job calls `Queue::release()` on the queue of the stage after the stage,
     *   its submit waits for the returned value and records the acquire half
     * - the same family: no release
     * @details
     * - returns at once, the present job waits for the stage
     * - transitions the image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` from its last state in the stage primary
     * - does not change the graph states
     * - `VK_ERROR_OUT_OF_DATE_KHR` -> the next `acquire()` recreates the swapchain
     * @pre `acquire()` returned an image, not presented or unacquired
     * @pre the stage recording uses the image
     * @pre the stage is the last use of the image in the frame or waits for the other uses
     * @pre `UNSUBMITTED` -> the preconditions of `Stage::submit()`
     * @see `PrimaryCmdBuffer::states()`
     */
    void present(Stage&);

    /**
     * gives the acquired image back without a present
     * @pre `acquire()` returned an image, not presented or unacquired
     * @pre no submit uses the image
     * @throws cvk::vk_result_exception result of `vkReleaseSwapchainImagesKHR()`
     */
    void unacquire();

    [[nodiscard]] constexpr bool created() const { return _surface.created(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @pre `created()`
     */
    [[nodiscard]] constexpr cvk::Swapchain const& swapchain() const {
        debug_check(*this);
        return *_swapchainOpt;
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
    /**
     * present job of `present()`
     * @details
     * - stage on the same queue or of another family -> off the strand: awaits `Semaphore::submitted()` of the stage
     *   semaphore with the frame number
     * - stage of another family -> `Queue::release()` of the image on the stage queue
     * - in the strand of the queue: records the command buffer, submits it through `Queue::vk()`, presents
     * - the submit waits for the semaphore of the stage and the release, signals the present semaphore and
     *   `Presenter::_semaphore`
     * - the command buffer: the acquire half of another family, the transition to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`
     * - stores its first error for `acquire()`
     * @param lastState of the image in the stage primary
     * @pre the command buffer of the submit belongs to the queue family of the queue
     */
    [[nodiscard]] cth::co::executor_task<void> presentJob(
        Queue const& stageQueue,
        Semaphore const& stageSemaphore,
        uint64_t frameNumber,
        uint32_t imageIdx,
        ImageState lastState
    );

    /**
     * awaits the present fence of the image without a block of the strand
     * @details exports `cvk::Swapchain::presentFence()`, awaits it with `cth::co::this_coro::await_handle()`
     */
    [[nodiscard]] cth::co::executor_task<void> awaitPresentFence(uint32_t imageIdx) const;

    cth::ref_wrapper<Core const> _core;
    cth::ref_wrapper<Queue const> _queue;
    Config _config;

    // destroyed after the swapchain
    cvk::Surface _surface;

    /**
     * constructed by `create()`, created once the surface extent is `> 0`
     */
    std::optional<cvk::Swapchain> _swapchainOpt = std::nullopt;

    /**
     * ring of `PresenterConfig::framesInFlight`, signaled by the acquires
     */
    std::vector<cvk::Semaphore> _acquireSemaphores{};

    size_t _acquireSemaphoreIdx = 0; ///< the semaphore in `Presenter::_acquireSemaphores` for the next acquire

    Semaphore _semaphore; ///< signaled by the present submits, each submit signals the next value

    std::vector<PresentSubmit> _submits{}; ///< one per swapchain image

    std::deque<uint32_t> _presentedImageIndices{}; ///< the images of the pending presents, the oldest first

    std::optional<uint32_t> _imageIdxOpt = std::nullopt; ///< the acquired image, not presented or unacquired

    /**
     * `VK_ERROR_OUT_OF_DATE_KHR` of a present, the next `acquire()` recreates the swapchain
     */
    std::atomic<bool> _outOfDate = false;

    std::mutex _errorMutex;
    std::exception_ptr _errorPtr = nullptr;

public:
    Presenter(Presenter const& other) = delete;
    Presenter(Presenter&& other) = delete;
    Presenter& operator=(Presenter const& other) = delete;
    Presenter& operator=(Presenter&& other) = delete;

    static constexpr void debug_check(Presenter const&);
};
}

namespace jvk {
constexpr void Presenter::debug_check(Presenter const& presenter) {
    CTH_CRITICAL(!presenter.created(), "presenter must be created") {}
}
}
