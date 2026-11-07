#pragma once
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/fence.hpp"

#include "cvk/render/sync/semaphore.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/coro/strand.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/os/fence.hpp>
#include <cth/ptr/not_null.hpp>

#include <atomic>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <vector>


namespace cvk {
class Image;
class ImageView;
class TimelineSemaphore;
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
 * @details one acquired image at a time
 * @attention not thread safe
 */
class Presenter {
public:
    //TODO review jvk::Core::create() passes no surfaces to cvk::Core::create(): no queue family gets PRESENT
    //TODO review surface extent 0 at the construction: the first acquire() with an extent > 0 creates the swapchain
    /**
     * creates the swapchain, the acquire semaphores and the present submits
     * @details the queue presents and runs the present submits
     * @details surface extent 0 -> the first @ref acquire() with an extent > 0 creates the swapchain
     * @pre @ref Core::created()
     * @pre @ref cvk::Surface::created()
     * @pre the queue can present to the surface
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of a create of the swapchain, semaphores, fences or command buffers
     */
    Presenter(Core const&, Queue const&, cvk::Surface, cvk::Swapchain::Config);

    /**
     * waits for the present submits, destroys the swapchain, then the surface
     * @pre no @ref acquire() is pending
     */
    ~Presenter();

    //TODO review how the first submit that uses the image finds the acquire semaphore through the tracking
    //TODO review the swapchain is externally synchronized: acquire() runs in the presenter strand,
    // the present in the strand of the queue, they need an order
    /**
     * acquires the next swapchain image in the strand of the presenter
     * @details VK_ERROR_OUT_OF_DATE_KHR of the last present or acquire, or a changed surface extent
     *  -> recreates the swapchain first
     * @details an acquired image without @ref present() or @ref release() -> releases it, warns
     * @details the first submit that uses the image waits for the acquire
     * @return std::nullopt -> surface extent 0, no image
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception other result of vkAcquireNextImageKHR() or vkCreateSwapchainKHR()
     * @throws cvk::vk_result_exception the first error of a present job
     * @attention blocks the strand of the presenter while no image is free
     */
    [[nodiscard]] cth::co::executor_task<std::optional<SwapchainImage>> acquire();

    //TODO review the present submit needs the tracked state of the image: access to the graph states through the stage
    /**
     * presents the acquired image after the stage
     * @details by @ref Stage::status():
     * - UNSUBMITTED: submits the stage, then presents
     * - SUBMITTED: presents
     * - SKIPPED: calls @ref release()
     * @details returns at once, the present job waits for the stage
     * @details the present submit transitions the image from its tracked state to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
     * @details VK_ERROR_OUT_OF_DATE_KHR -> the next @ref acquire() recreates the swapchain
     * @pre @ref acquire() returned an image, not presented or released
     * @pre the stage is the last use of the image in the frame or waits for the other uses
     * @pre UNSUBMITTED -> the preconditions of @ref Stage::submit()
     */
    void present(Stage&);

    /**
     * gives the acquired image back without a present
     * @pre @ref acquire() returned an image, not presented or released
     * @pre no submit uses the image
     * @throws cvk::vk_result_exception result of vkReleaseSwapchainImagesKHR()
     */
    void release();

    [[nodiscard]] constexpr cvk::Swapchain const& swapchain() const { return _swapchain; }

private:
    /**
     * submit before the present of one swapchain image
     */
    struct PresentSubmit {
        /**
         * the transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, with the acquire of another queue family
         */
        PrimaryCmdBuffer cmd;

        Fence fence;
    };

    /**
     * present job, see @ref present()
     * @details waits for the submitted mark of the stage off the strands
     * @details in the strand of the queue: submits with a wait for the timeline value and a signal of the present
     *  semaphore, then presents
     * @details stores its first error for @ref acquire()
     */
    [[nodiscard]] cth::co::executor_task<void> presentJob(
        cvk::TimelineSemaphore const&,
        uint64_t timelineValue,
        cth::os::fence const& submitted,
        uint32_t imageIdx
    );

    cth::not_null_ptr<Core const> _corePtr;
    cth::not_null_ptr<Queue const> _queuePtr;

    //TODO review the scheduler of the strand: jvk::Core exposes none, a blocked acquire blocks one of its threads
    cth::co::strand _strand;

    // destroyed after the swapchain
    cvk::Surface _surface;
    cvk::Swapchain _swapchain;

    //TODO review count and reuse: a released image leaves its acquire semaphore pending
    /**
     * signaled by the acquires, free again after the fence of the submit that waited for it
     */
    std::vector<cvk::Semaphore> _acquireSemaphores{};

    //TODO review the present semaphores come from cvk::Swapchain::presentSemaphore(), one per image
    /**
     * one per swapchain image
     */
    std::vector<PresentSubmit> _submits{};

    /**
     * the acquired image, not presented or released
     */
    std::optional<uint32_t> _imageIdxOpt = std::nullopt;

    /**
     * VK_ERROR_OUT_OF_DATE_KHR of a present, the next @ref acquire() recreates the swapchain
     */
    std::atomic<bool> _outOfDate = false;

    //TODO review the errors of the present jobs go to acquire(), the presenter knows no graph
    std::mutex _errorMutex;
    std::exception_ptr _errorPtr = nullptr;

public:
    Presenter(Presenter const& other) = delete;
    Presenter(Presenter&& other) = delete;
    Presenter& operator=(Presenter const& other) = delete;
    Presenter& operator=(Presenter&& other) = delete;
};
}
