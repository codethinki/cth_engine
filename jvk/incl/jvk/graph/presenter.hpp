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
#include <cth/os/fence.hpp>
#include <cth/ptr/not_null.hpp>

#include <atomic>
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
//TODO review cvk types: the swapchain owns cvk::Image values, a jvk::Image& to them is not possible
/**
 * acquired swapchain image
 * @details tracked like every image: the commands that use it record its accesses
 * @details valid until the next @ref Presenter::acquire()
 */
struct SwapchainImage {
    cth::not_null_ptr<cvk::Image const> imagePtr;
    cth::not_null_ptr<cvk::ImageView const> viewPtr;

    /**
     * index of the image in the swapchain
     */
    uint32_t imageIdx;

    /**
     * extent of the swapchain at the acquire
     */
    VkExtent2D extent;
};

/**
 * presents to a surface
 * @details owns the surface and the swapchain
 * @details recreates the swapchain in @ref acquire()
 * @details acquires, submits and presents in the strand of its queue
 * @details one acquired image at a time
 * @attention not thread safe
 */
class Presenter {
public:
    /**
     * base constructor
     * @details the queue presents and runs the present submits
     * @pre @ref Core::created()
     */
    Presenter(Core const&, Queue const&, cvk::Swapchain::Config);

    /**
     * constructs and creates
     * @details calls @ref Presenter(Core const&, Queue const&, cvk::Swapchain::Config), @ref create()
     */
    Presenter(Core const&, Queue const&, cvk::Swapchain::Config, cvk::Surface);

    /**
     * @note calls @ref optDestroy()
     */
    ~Presenter() { optDestroy(); }

    //TODO review cvk::Surface::extent() (2026-11-08) does not exist yet: create() and acquire() need the extent
    /**
     * takes the surface, creates the swapchain, the semaphores and the command buffers
     * @details surface extent 0 -> the first @ref acquire() with an extent > 0 creates the swapchain
     * @pre @ref cvk::Surface::created()
     * @pre the queue can present to the surface
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of a create of the swapchain, semaphores or command buffers
     */
    void create(cvk::Surface);

    /**
     * waits for the present jobs, destroys the swapchain, then the surface, and forgets
     * @details destroys the semaphores and the command buffers
     * @pre @ref created()
     * @pre no @ref acquire() is pending
     * @pre the acquired image is presented or unacquired
     */
    void destroy();

    /**
     * if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * acquires the next swapchain image for the stage
     * @details adds the wait for the acquire to the stage, see @ref Stage::addWait()
     * @details sets the graph state of the image to VK_IMAGE_LAYOUT_UNDEFINED with the wait stages, see
     *  @ref Stage::assume(): the first barrier of the image chains with the wait
     * @details VK_ERROR_OUT_OF_DATE_KHR of the last present or acquire, or a changed surface extent
     *  -> recreates the swapchain first
     * @details all images in use -> awaits the oldest present fence first, the strand of the queue does not block
     * @details an acquired image without @ref present() or @ref unacquire() -> unacquires it, warns
     * @param stages pipeline stages of the stage that wait for the acquire
     * @return std::nullopt -> surface extent 0, no image, no wait
     * @pre @ref created()
     * @pre @ref Stage::status() is UNSUBMITTED
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception other result of vkAcquireNextImageKHR() or vkCreateSwapchainKHR()
     * @throws cvk::vk_result_exception the first error of a present job
     */
    [[nodiscard]] cth::co::executor_task<std::optional<SwapchainImage>> acquire(
        Stage&,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    );

    //TODO review a present queue of another family than the stage (EXCLUSIVE sharing): the release barrier
    // belongs into the submit of the stage, SUBMITTED is too late for it
    /**
     * presents the acquired image after the stage
     * @details by @ref Stage::status():
     * - UNSUBMITTED: submits the stage, then presents
     * - SUBMITTED: presents
     * - SKIPPED: calls @ref unacquire()
     * @details returns at once, the present job waits for the stage
     * @details the present submit transitions the image to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR from its last state in
     *  the stage recording (@ref PrimaryCmdBuffer::states(), the executed secondaries included)
     * @details does not change the graph states, the next @ref acquire() of the image sets them
     * @details VK_ERROR_OUT_OF_DATE_KHR -> the next @ref acquire() recreates the swapchain
     * @pre @ref acquire() returned an image, not presented or unacquired
     * @pre the stage recording uses the image
     * @pre the stage is the last use of the image in the frame or waits for the other uses
     * @pre UNSUBMITTED -> the preconditions of @ref Stage::submit()
     */
    void present(Stage&);

    /**
     * gives the acquired image back without a present
     * @pre @ref acquire() returned an image, not presented or unacquired
     * @pre no submit uses the image
     * @throws cvk::vk_result_exception result of vkReleaseSwapchainImagesKHR()
     */
    void unacquire();

    [[nodiscard]] constexpr bool created() const { return _surface.created(); }

    /**
     * @pre @ref created()
     */
    [[nodiscard]] constexpr cvk::Swapchain const& swapchain() const {
        debug_check(*this);
        return *_swapchainOpt;
    }

private:
    /**
     * submit before the present of one swapchain image
     * @details signals the present semaphore of the image, see @ref cvk::Swapchain::presentSemaphore()
     */
    struct PresentSubmit {
        /**
         * the barrier from the last state of the image in the stage recording to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
         */
        PrimaryCmdBuffer cmd;

        /**
         * value of @ref _semaphore that the last submit of @ref cmd signals, the command buffer is free once reached
         */
        uint64_t value = 0;
    };

    /**
     * present job, see @ref present()
     * @details the stage on the same queue -> awaits the submitted mark of the stage first, off the strand
     * @details in the strand of the queue: records the command buffer, submits it through @ref Queue::vk(),
     *  presents
     * @details the submit waits for the semaphore of the stage, signals the present semaphore and @ref _semaphore
     * @details stores its first error for @ref acquire()
     * @param lastState of the image in the stage recording
     */
    [[nodiscard]] cth::co::executor_task<void> presentJob(
        Queue const& stageQueue,
        Semaphore const& stageSemaphore,
        uint64_t frameNumber,
        cth::os::fence const& submitted,
        uint32_t imageIdx,
        ImageState lastState
    );

    //TODO review posix: the export resets the fence, cvk::Swapchain::present() of the image and the destruction
    // then wait for it forever: signal it again (empty submit) or let cvk keep the state (import the sync fd back)
    /**
     * awaits the present fence of the image, the strand of the queue does not block
     * @details exports @ref cvk::Swapchain::presentFence(), awaits it with cth::co::this_coro::await_handle()
     */
    [[nodiscard]] cth::co::executor_task<void> awaitPresentFence(uint32_t imageIdx) const;

    cth::not_null_ptr<Core const> _corePtr;
    cth::not_null_ptr<Queue const> _queuePtr;
    cvk::Swapchain::Config _config;

    // destroyed after the swapchain
    cvk::Surface _surface;

    /**
     * constructed by @ref create(), created once the surface extent is > 0
     */
    std::optional<cvk::Swapchain> _swapchainOpt = std::nullopt;

    //TODO review not applied yet: a ring of frames in flight semaphores, frames in flight in a presenter config
    // (2026-11-09 answers)
    /**
     * signaled by the acquires, free again after the gpu finished the submit that waited for it
     */
    std::vector<cvk::Semaphore> _acquireSemaphores{};

    /**
     * signaled by the present submits, each submit signals the next value
     */
    Semaphore _semaphore;

    /**
     * one per swapchain image
     */
    std::vector<PresentSubmit> _submits{};

    /**
     * the images of the pending presents, the oldest first
     */
    std::deque<uint32_t> _presentedImageIndices{};

    /**
     * the acquired image, not presented or unacquired
     */
    std::optional<uint32_t> _imageIdxOpt = std::nullopt;

    /**
     * VK_ERROR_OUT_OF_DATE_KHR of a present, the next @ref acquire() recreates the swapchain
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
