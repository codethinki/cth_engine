#pragma once
#include "jvk/base/queue.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/coro/counter_event.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstdint>
#include <memory>
#include <optional>


namespace cvk {
class Image;
}

namespace jvk::dev {
/**
 * state of a `Surface` that the pending present of its last image resolves
 * @details written before `PresentState::resolved`, `Surface::acquire()` reads it after its wait
 */
struct PresentState {
    /** value of the last resolved image of the surface, the images count from `1` */
    cth::co::counter_event resolved{};

    /** set by an out of date acquire or present, `Surface::acquire()` recreates the swapchain */
    bool outOfDate = false;

    /**
     * the last image was dropped: `Surface::acquire()` unacquires it
     * @details the gpu finished the stages that used it before its resolve
     */
    bool dropped = false;

    /**
     * resolves the image as presented
     * @details `VK_ERROR_OUT_OF_DATE_KHR` -> sets `PresentState::outOfDate`
     * @param value of the image
     * @param result of the present of the image
     * @pre the images before the image are resolved
     */
    void present(uint64_t value, VkResult result);

    /**
     * resolves the image as dropped: the next `Surface::acquire()` unacquires it
     * @param value of the image
     * @pre the images before the image are resolved
     * @pre the gpu finished the stages that used the image, or no stage submitted a use of it
     */
    void drop(uint64_t value);
};

/**
 * deleter of a pending present: drops the image
 */
struct present_dropper {
    uint64_t value; ///< of the image in `PresentState::resolved`

    void operator()(PresentState* state) const { state->drop(value); }
};

/**
 * present of an acquired swapchain image after the submit of its present stage
 * @details
 * - `Surface::acquire()` hands it to the present stage, see `Stage::present()`
 * - the submit job of the present stage presents the image, a skip drops it
 * - resolves the image exactly once: `present()`, `drop()`, or its destruction as dropped
 * - destroyed pending: only after the gpu finished the stages that used the image, e.g. in
 *   `~StageGraph()`, or before a stage submitted a use of it
 * - a moved-from present resolves nothing
 */
class PendingPresent {
public:
    /**
     * @param state of the surface
     * @param value of the image in `PresentState::resolved`
     * @param queue of the surface, presents the image
     * @param swapchain the current swapchain of the surface
     * @param imageIdx of the acquired image
     */
    PendingPresent(
        PresentState& state,
        uint64_t value,
        Queue const& queue,
        cvk::Swapchain& swapchain,
        uint32_t imageIdx
    ) :
        _state{&state, present_dropper{.value = value}},
        _queue{queue},
        _swapchain{swapchain},
        _imageIdx{imageIdx} {}

    /**
     * resolves the image as presented
     * @param result of the present of the image
     * @pre not resolved
     * @see `PresentState::present()`
     */
    void present(VkResult result) {
        debug_check(*this);
        auto const value = _state.get_deleter().value;
        _state.release()->present(value, result);
    }

    /**
     * resolves the image as dropped
     * @pre not resolved
     * @pre the gpu finished the stages that used the image
     * @see `PresentState::drop()`
     */
    void drop() {
        debug_check(*this);
        _state.reset();
    }

    [[nodiscard]] Queue const& queue() const { return *_queue; }
    [[nodiscard]] PresentImage image() const { return {_swapchain, _imageIdx}; }

    /**
     * @return the image in the swapchain
     */
    [[nodiscard]] cvk::Image const& swapchainImage() const { return _swapchain->image(_imageIdx); }

private:
    std::unique_ptr<PresentState, present_dropper> _state; ///< `nullptr` -> resolved or moved from
    cth::ref_wrapper<Queue const> _queue;
    cth::ref_wrapper<cvk::Swapchain> _swapchain;
    uint32_t _imageIdx;

public:
    PendingPresent(PendingPresent const& other) = delete;
    PendingPresent(PendingPresent&& other) noexcept = default;
    PendingPresent& operator=(PendingPresent const& other) = delete;
    PendingPresent& operator=(PendingPresent&& other) noexcept = default;

    static void debug_check(PendingPresent const&);
};
}

namespace jvk::dev {
inline void PendingPresent::debug_check(PendingPresent const& present) {
    CTH_CRITICAL(present._state == nullptr, "the present must be pending") {}
}
}
