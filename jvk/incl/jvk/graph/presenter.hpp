#pragma once
#include "jvk/sync/semaphore.hpp"

#include "cvk/render/sync/semaphore.hpp"
#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/coro/counter_event.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/data/hash_map.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>


namespace cvk {
class Image;
class ImageView;
}

namespace jvk {
class Queue;

/**
 * orchestrates the images of one swapchain
 * @details
 * - `acquire()` hands out an `AcquiredImage`, `present()` or `drop()` hands it back
 * - the presents follow the acquire order
 * - a dropped image is a spare: the next `acquire()` hands it out first
 * - owns `framesInFlight()` acquire semaphores and one ready semaphore per image: the ready
 *   points of the drops
 * - thread safe: `acquire()`, `present()` and `drop()` from any thread
 * - the strand of the queue runs the vulkan calls of the swapchain, except the fallback of
 *   `acquire()`: it excludes the presents and the tries of the presenter
 * - the destructor releases the spares, see `cvk::Swapchain::unacquire()`
 */
class Presenter {
public:
    class AcquiredImage;

    /**
     * wait of the first use of an image: its acquire semaphore or the ready point of its drop
     */
    using wait_t = std::variant<cth::ref_wrapper<cvk::Semaphore const>, SemaphoreSubmitInfo>;

    // no base constructor and create(): the presenter has no wrap or release
    /**
     * creates the acquire semaphores and the ready semaphores
     * @param presentQueue presents the images, its strand runs the tries and the presents
     * @param swapchain of the images
     * @param framesInFlight count of the acquire semaphores
     * @details reads `minImageCount` of the surface for the fallback of `acquire()`
     * @pre `cvk::Swapchain::created()`
     * @pre `swapchain` presents through `presentQueue`
     * @pre `framesInFlight > 0`
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()` or
     *  `vkGetPhysicalDeviceSurfaceCapabilitiesKHR()`
     */
    Presenter(Queue& presentQueue, cvk::Swapchain& swapchain, uint32_t framesInFlight);

    /**
     * releases the spares, destroys the acquire semaphores
     * @details a failed release does not stop the destruction
     * @pre every `AcquiredImage` of the presenter is handed back
     * @pre no `acquire()` and no `present()` is pending
     * @pre the ready points of the spares are reached:
     *  VUID-VkReleaseSwapchainImagesInfoKHR-pImageIndices-07786
     * @pre the submits that wait for the acquire semaphores or signal the ready semaphores
     *  completed: VUID-vkDestroySemaphore-semaphore-05149
     * @see `finished()`
     */
    ~Presenter();

    /**
     * hands out the next image of the swapchain
     * @details the image:
     * - a spare first
     * - else the try: `vkAcquireNextImageKHR()` with timeout `0` in the strand of the queue
     * - the try returns `VK_NOT_READY` -> the fallback: waits until no present of the presenter
     *   is pending, then `vkAcquireNextImageKHR()` on a driver thread
     * - the fallback blocks only with at most `imageCount - minImageCount` of the surface acquired
     *   images, else it waits for the present fences, then tries:
     *   VUID-vkAcquireNextImageKHR-swapchain-01802
     * - `VK_SUBOPTIMAL_KHR` counts as success
     * @details waits for a hand back:
     * - every acquire semaphore is with an image that is not presented or dropped
     * - the try of the fallback returns `VK_NOT_READY`
     * @details no free acquire semaphore:
     * - a presented image -> the fallback waits for its present fence
     * - else waits for the ready point of a drop, see `Semaphore::wait()`
     * @return the image, `std::nullopt` -> out of date: the acquire or a present returned
     *  `VK_ERROR_OUT_OF_DATE_KHR`, every later acquire returns `std::nullopt`
     * @pre the swapchain is not retired: VUID-vkAcquireNextImageKHR-swapchain-01285
     * @throws std::exception the first failure of a present before, once
     * @throws cvk::vk_result_exception other result of `vkAcquireNextImageKHR()`
     * @throws cvk::vk_result_exception result of `vkGetFenceStatus()`, `vkWaitForFences()` or
     *  `vkGetSemaphoreCounterValue()`
     * @throws cth::except::coro_exception reason: the wait for a ready point failed
     * @attention a hand back that never comes -> waits forever, e.g. the caller holds
     *  `framesInFlight()` images
     */
    [[nodiscard]] cth::co::executor_task<std::optional<AcquiredImage>> acquire();

    /**
     * hands the image back for its present
     * @details
     * - waits until every image acquired before is handed back: the presents follow the acquire
     *   order
     * - waits for the end of a fallback of `acquire()`
     * - waits for the present fence of the image while the last present of the image left it
     *   pending: the present resets it
     * - one `vkQueuePresentKHR()` in the strand of the queue, see `Queue::present()`
     * - ends after `vkQueuePresentKHR()`
     * - never throws: the next `acquire()` rethrows the first failure,
     *   `VK_ERROR_OUT_OF_DATE_KHR` -> it returns `std::nullopt`
     * @details the fence of the present:
     * - pending once the present is enqueued: `VK_SUCCESS`, `VK_SUBOPTIMAL_KHR` and the rejected
     *   presents `VK_ERROR_OUT_OF_DATE_KHR`, `VK_ERROR_SURFACE_LOST_KHR`,
     *   `VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT`, `VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT`,
     *   see `vkQueuePresentKHR()`
     * - another failure enqueues nothing: the image stays acquired and its acquire semaphore
     *   held until the teardown, `finished()` stays `false`: these results are out of memory,
     *   device lost or unknown, practically fatal, the next `acquire()` rethrows the error
     * @pre the image is of this presenter
     * @pre the image is in `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`
     * @pre a submitted batch signals `AcquiredImage::presentSemaphore()`, after the uses of the
     *  image and after the wait for `AcquiredImage::wait()`
     */
    [[nodiscard]] cth::co::executor_task<void> present(AcquiredImage image);

    /**
     * hands the image back without a present: the image becomes a spare that waits for
     * `AcquiredImage::ready()`
     * @details the acquire semaphore of the image is free once the ready point is reached
     * @details the destruction of an image drops it without a ready point: the spare keeps
     *  `AcquiredImage::wait()`
     * @pre the image is of this presenter
     * @pre a batch signals `AcquiredImage::ready()`, after the uses of the image and after the
     *  wait for `AcquiredImage::wait()`
     */
    void drop(AcquiredImage image);

    /**
     * checks without a wait that the presenter has no work left
     * @return every `AcquiredImage` is handed back, no present is pending, every ready point is
     *  reached, no present fence is pending, every present was enqueued
     * @pre no `acquire()` runs at the same time: a present of its image resets a present fence
     * @throws cvk::vk_result_exception result of `vkGetSemaphoreCounterValue()` or
     *  `vkGetFenceStatus()`
     */
    [[nodiscard]] bool finished() const;

    /**
     * blocks until no present fence of the swapchain is pending
     * @details for the teardown: `cvk::Swapchain::destroy()` waits for no fence
     * @pre no `present()` runs at the same time
     * @throws cvk::vk_result_exception result of `vkWaitForFences()`, e.g. `VK_ERROR_DEVICE_LOST`
     */
    void waitPresents();

    [[nodiscard]] constexpr Queue& queue() const { return *_queue; }
    [[nodiscard]] constexpr cvk::Swapchain& swapchain() const { return *_swapchain; }
    [[nodiscard]] constexpr uint32_t framesInFlight() const {
        return static_cast<uint32_t>(_acquireSemaphores.size());
    }

private:
    struct Free {};

    /** with an image of a caller or a spare */
    struct Held {};

    /** free once the present fence of the image signals */
    struct Presented {
        uint32_t imageIdx;
    };

    /**
     * state of an acquire semaphore, a `SemaphoreSubmitInfo`: free once the ready point of the
     * drop is reached
     */
    using reuse_t = std::variant<Free, Held, Presented, SemaphoreSubmitInfo>;

    struct Spare {
        uint32_t imageIdx;
        wait_t wait;
    };

    /** the state changed during the try, `acquire()` checks it again */
    struct Retry {};

    struct Fallback {
        /** `std::nullopt` -> the fallback waits for a present fence */
        std::optional<size_t> semaphoreIdxOpt;
        /** count of the started presents at the start, the fallback waits for their end */
        uint64_t presents;
    };

    /** result of a try: the image, a retry, the ready point to wait for or the fallback */
    using try_t = std::variant<AcquiredImage, Retry, SemaphoreSubmitInfo, Fallback>;

    /**
     * the semaphore choice and the try in the strand
     * @details `VK_NOT_READY` or only present fences to wait for -> starts the fallback
     */
    [[nodiscard]] cth::co::executor_task<try_t> tryInStrand();

    /**
     * @return the image, `std::nullopt` -> retry
     */
    [[nodiscard]] cth::co::executor_task<std::optional<AcquiredImage>> fallback(Fallback);

    /**
     * frees the acquire semaphores whose ready point or present fence is reached
     * @return a free acquire semaphore, `std::nullopt` -> none
     * @pre in the strand or in the fallback: the presents reset the present fences
     */
    [[nodiscard]] std::optional<size_t> recycle();

    /**
     * blocks until the present fence of a presented image signals
     * @return the free acquire semaphore
     * @pre in the fallback, an acquire semaphore is `Presented`
     */
    [[nodiscard]] size_t awaitPresentFence();

    /**
     * @return the images whose present fence is pending, locks
     */
    [[nodiscard]] std::vector<uint32_t> pendingFences() const;

    /**
     * blocks until the present fences of the images signaled, then counts them as not pending
     * @pre no present of the images runs at the same time: it resets the fence
     */
    void waitPresents(std::span<uint32_t const> imageIndices);

    /**
     * @return the failure of a present, `nullptr` -> none, the presenter forgets it
     */
    [[nodiscard]] std::exception_ptr takeError();

    /**
     * hands the image back as a spare
     * @param readyOpt `std::nullopt` -> the spare keeps the wait
     */
    void spare(
        uint32_t imageIdx,
        uint64_t acquireNumber,
        wait_t const& wait,
        std::optional<SemaphoreSubmitInfo> const& readyOpt
    );

    // the helpers below lock nothing, the caller holds _mutex
    [[nodiscard]] AcquiredImage handOut(uint32_t imageIdx, wait_t wait);
    [[nodiscard]] Fallback startFallback(std::optional<size_t> semaphoreIdxOpt);
    void endFallback();
    /** advances `_handedBack` over the hand backs in acquire order */
    void handBack(uint64_t acquireNumber);

    [[nodiscard]] size_t semaphoreIdx(cvk::Semaphore const& semaphore) const {
        return static_cast<size_t>(&semaphore - _acquireSemaphores.data());
    }

    cth::ref_wrapper<Queue> _queue;
    cth::ref_wrapper<cvk::Swapchain> _swapchain;

    /** `imageCount - minImageCount` of the surface: more acquired images -> no blocking acquire */
    uint32_t _blockingAcquireLimit;
    std::vector<cvk::Semaphore> _acquireSemaphores{};
    /**
     * one per image, a batch signals the acquire number of a drop, see `AcquiredImage::ready()`
     * @details the batches of one image follow its acquires: the values rise
     */
    std::vector<Semaphore> _readySemaphores{};

    mutable std::mutex _mutex{};

    // behind _mutex
    std::vector<reuse_t> _reuses; ///< one per acquire semaphore
    std::vector<Spare> _spares{};
    /** acquire number of the last handed out image, the numbers start at `1` */
    uint64_t _acquires = 0;
    /** handed back while an image acquired before is not */
    cth::dt::flat_hash_set<uint64_t> _handedBackAhead{};
    /** acquired from the swapchain, not presented or released: the images of callers and spares */
    uint32_t _acquiredImages = 0;
    /**
     * per image: an enqueued present left its fence pending, the fence did not signal since
     * @details the swapchain resets the fence at the next present: the present waits for it first
     */
    std::vector<bool> _pendingFences;
    /**
     * presents that failed without an enqueue: their images stay acquired, no end of the gpu work
     * of their submits is known
     */
    uint32_t _unenqueuedPresents = 0;
    uint64_t _presentsStarted = 0;
    uint64_t _fallbacks = 0; ///< count of the started fallbacks
    bool _fallback = false; ///< a fallback runs
    bool _outOfDate = false;
    std::exception_ptr _error = nullptr; ///< first failure of a present

    // set behind _mutex
    /** every image up to the acquire number is handed back, a present after its end */
    cth::co::counter_event _handedBack{};
    cth::co::counter_event _handBacks{}; ///< count of the hand backs
    cth::co::counter_event _presentsDone{}; ///< count of the ended presents
    cth::co::counter_event _fallbacksDone{}; ///< count of the ended fallbacks

public:
    Presenter(Presenter const& other) = delete;
    Presenter(Presenter&& other) = delete;
    Presenter& operator=(Presenter const& other) = delete;
    Presenter& operator=(Presenter&& other) = delete;
};
}

namespace jvk {
/**
 * acquired swapchain image of a `Presenter` while the engine holds it, handed back exactly once
 * @details
 * - `Presenter::present()` or `Presenter::drop()` hands it back, the destruction drops it
 *   without a ready point
 * - the content is undefined: the first use transitions it from `VK_IMAGE_LAYOUT_UNDEFINED`
 * - the first use waits for `wait()`
 * - a moved-from image hands back nothing
 */
class Presenter::AcquiredImage {
public:
    /**
     * constructs the image of an acquire, `Presenter::acquire()` constructs it
     * @param presenter of the image
     * @param imageIdx index of the image in the swapchain
     * @param acquireNumber order of the acquire, the presents follow it
     * @param wait of the first use
     */
    AcquiredImage(Presenter& presenter, uint32_t imageIdx, uint64_t acquireNumber, wait_t wait) :
        _presenter{
            &presenter,
            dropper{.imageIdx = imageIdx, .acquireNumber = acquireNumber, .wait = std::move(wait)}
        } {}

    /**
     * @return the presenter of the image
     * @pre not handed back
     */
    [[nodiscard]] Presenter& presenter() const {
        debug_check(*this);
        return *_presenter;
    }

    /**
     * @return the image in the swapchain
     * @pre not handed back
     */
    [[nodiscard]] cvk::Image const& image() const {
        return presenter().swapchain().image(imageIdx());
    }

    /**
     * @pre not handed back
     */
    [[nodiscard]] cvk::ImageView const& view() const {
        return presenter().swapchain().view(imageIdx());
    }

    /**
     * @pre not handed back
     */
    [[nodiscard]] VkExtent2D extent() const { return presenter().swapchain().extent(); }

    /**
     * @return the semaphore that the present waits for, a submit signals it
     * @pre not handed back
     */
    [[nodiscard]] cvk::Semaphore const& presentSemaphore() const {
        return presenter().swapchain().presentSemaphore(imageIdx());
    }

    /**
     * @return wait of the first use, the ready point keeps the stages of its drop
     */
    [[nodiscard]] wait_t const& wait() const { return _presenter.get_deleter().wait; }

    /**
     * @return the ready point of a drop: the ready semaphore of the image with `acquireNumber()`
     * @details a batch after the uses of the image and after the wait for `wait()` signals it, the
     *  spare of `Presenter::drop()` waits for it
     * @pre not handed back
     */
    [[nodiscard]] SemaphoreSubmitInfo ready() const {
        return SemaphoreSubmitInfo{
            .semaphore = presenter()._readySemaphores[imageIdx()],
            .value = acquireNumber(),
        };
    }

    /**
     * @return index of the image in the swapchain
     */
    [[nodiscard]] uint32_t imageIdx() const { return _presenter.get_deleter().imageIdx; }

    /**
     * @return order of the acquire, the numbers start at `1`
     */
    [[nodiscard]] uint64_t acquireNumber() const { return _presenter.get_deleter().acquireNumber; }

private:
    // the presenter owns the acquired image: its present() and drop() take the hand back
    friend class Presenter;

    /**
     * deleter of the image: drops it without a ready point
     */
    struct dropper {
        uint32_t imageIdx;
        uint64_t acquireNumber;
        wait_t wait;

        void operator()(Presenter* presenter) const {
            presenter->spare(imageIdx, acquireNumber, wait, std::nullopt);
        }
    };

    std::unique_ptr<Presenter, dropper> _presenter; ///< `nullptr` -> handed back or moved from

public:
    AcquiredImage(AcquiredImage const& other) = delete;
    AcquiredImage(AcquiredImage&& other) noexcept = default;
    AcquiredImage& operator=(AcquiredImage const& other) = delete;
    AcquiredImage& operator=(AcquiredImage&& other) noexcept = default;

    static void debug_check(AcquiredImage const&);
};
}

namespace jvk {
inline void Presenter::AcquiredImage::debug_check(AcquiredImage const& image) {
    CTH_CRITICAL(image._presenter == nullptr, "the image must not be handed back") {}
}
}
