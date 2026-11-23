#pragma once
#include "frame.hpp"
#include "stage.hpp"
#include "stage_graph_config.hpp"

#include "jvk/base/stasis.hpp"
#include "jvk/sync/resource_tracker.hpp"

#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cth/coro/catching_scope.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/data/hash_map.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
#include <utility>
#include <vector>


namespace cvk {
class BaseBuffer;
class Image;
}

namespace jvk {
class Core;
class Queue;
}

namespace jvk {
/**
 * stages with their waits, once per frame, in frame slots
 * @details
 * - one `Semaphore` per stage slot: the submit or the skip of frame N signals N
 * - the frame numbers start at `firstFrameNumber()`, the semaphores at the number before: a wait for a frame before
 *   the first frame is reached from the start
 * - the stages of a frame record at the same time, on any thread
 * - the submits of a stage follow the frame order
 * - the resource states after all submits resolve the first uses of each submit in its prologue
 * - a resource of another queue family: the last queue releases it, the prologue acquires it
 */
class StageGraph {
public:
    using Config = StageGraphConfig;

    // no base constructor and create(): the graph has no wrap or release
    /**
     * creates the stage slots and the frame slots
     * @details computes `firstFrameNumber()` from the config
     * @pre `Core::created()`
     * @pre the config passes `Config::debug_check()`
     * @throws cvk::vk_result_exception result of a create of the semaphores or command buffers
     */
    StageGraph(Core&, Config);

    /**
     * waits until the gpu finished the submits of every slot, destroys the slots
     * @details
     * - skips the unsubmitted stages first, e.g. after an exception in their recording: their waiters go on
     * - the skip drops the presents of the stage, see `Surface::acquire()`
     * - waits for the end of every submission
     * - a failed skip or wait does not stop the destruction
     * @attention blocks the calling thread: a worker of the core must be free for the submissions
     */
    ~StageGraph();

    // awaits Stage::submitted() of the old frame: the gpu can finish before the submission marks
    // the submit
    /**
     * constructs the frame and its stages in the next slot
     * @details the frame before: skips its stages without `Stage::submit()` or `Stage::skip()`, warns for a waited one
     * @details the old frame of the slot: awaits `StageSlot::submitDone` of its stages: their
     *  submissions ended the submits and the presents, then the stages: the gpu finished them, both
     *  with `cth::co::all()`
     * @return the frame of the slot
     * @pre no stage of the frame before records or submits: `nextFrame()` ends the frame
     * @throws std::exception the first error of a submission or a scope, stored before the call: an
     *  error during the waits -> the next `nextFrame()` rethrows it, see `cth::co::catching_scope`
     * @throws cth::except::group_coro_exception reason: the wait for a stage of the old frame
     *  failed, after the waits for all its stages: `exceptions()` holds the
     *  `cth::except::coro_exception` per stage
     * @attention not thread safe
     */
    [[nodiscard]] cth::co::executor_task<Frame&> nextFrame();

    /**
     * submits the primary of the stage
     * @details
     * - sets the status of the stage to `SUBMITTED`
     * - thread safe for different stages
     * - returns at once, the strand of the queue submits after the waited stages submitted
     * - resolves the first uses of the primary against the resource states after all submits
     * - an unknown image: the prologue transitions it from `VK_IMAGE_LAYOUT_UNDEFINED`
     * - an `EXCLUSIVE` resource of another queue family: `Queue::transfer()` of the release half on
     *   its last queue, the prologue acquires it
     * - waits for the stage waits of the config and the waits of `Stage::await()`
     * - signals `Stage::semaphore()` with `Stage::frameNumber()`
     * - the images of `Stage::present()`: the epilogue after the primary transitions them, the
     *   presents follow the submit, see `Surface::acquire()`
     * - an error: skips, `nextFrame()` rethrows the error
     * - after the gpu finished: destroys the retained objects
     * @pre the stage is of this graph
     * @pre `Stage::begin()` in this frame
     * @pre `Stage::status()` is `UNSUBMITTED`
     * @pre the primary and its executed secondaries belong to the queue family of `Stage::queue()`
     * @note ends the rendering and the primary
     */
    void submit(Stage&);

    /**
     * submits no work for the stage in this frame
     * @details sets the status of the stage to `SKIPPED`, thread safe for different stages
     * @details returns at once, the strand of the queue submits after the waited stages submitted
     * @details skip submit on the queue of the stage, no command buffers:
     * - waits for the stage waits of the config and the waits of `Stage::await()`
     * - signals `Stage::semaphore()` with `Stage::frameNumber()`
     * @details drops the images of `Stage::present()`, see `Surface::acquire()`
     * @details after the gpu finished the skip submit: destroys the retained objects
     * @details a begun primary: ends its rendering and the primary, discards the recording
     * @pre the stage is of this graph
     * @pre `Stage::status()` is `UNSUBMITTED`
     */
    void skip(Stage&);

    /**
     * sets the state of the image after all submits, without a barrier
     * @details
     * - the first use of the next submit starts from the state
     * - thread safe
     * @pre the submissions before that use the image appended their states: they are marked
     * @see `ResourceTracker::assume()`
     */
    void assume(cvk::Image const&, ImageState const& state);

    /**
     * removes the state and the last queue of the image
     * @details
     * - call it before the image is destroyed: a new image can get its handle
     * - `Stage::retain()` does it for the retained images
     * - thread safe
     * @pre the gpu finished the last submit that uses the image
     */
    void erase(cvk::Image const&);

    /**
     * removes the state and the last queue of the buffer
     * @details the details of @ref erase(cvk::Image const&)
     * @pre the gpu finished the last submit that uses the buffer
     */
    void erase(cvk::BaseBuffer const&);

    /**
     * @return number of the first frame: `1 +` the largest `StageWait::frameOffset` of the config
     */
    [[nodiscard]] constexpr uint64_t firstFrameNumber() const { return _firstFrameNumber; }

    [[nodiscard]] constexpr Core& core() const { return *_core; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    using image_mip_t = std::pair<VkImage, uint32_t>;

    template<class Key>
    using last_queue_map_t = cth::dt::flat_hash_map<Key, cth::ref_wrapper<Queue>>;

    using release_map_t = cth::dt::flat_hash_map<Queue*, cvk::Barrier>;

    void skipUnsubmitted(dev::FrameSlot&);

    //TODO review one helper for submit() and skip(): they differ in the status only
    void detachSubmission(Stage&, Stage::Status targetStatus);

    // no marks as parameters: the submission awaits Semaphore::submitted() of the slots, they live
    // as long as the graph
    // next() before the mark: the submission of the next frame of the slot awaits the mark, it
    // reads the values
    [[nodiscard]] cth::co::executor_task<void> submission(
        dev::StageSlot&,
        uint64_t frameNumber,
        dev::RetainedResources retained,
        Stasis::pos stasisEnd,
        Stage::Status targetStatus
    );

    /** the caller holds `_statesMutex` */
    void append(
        ResourceTracker const& recording,
        Queue& queue,
        cvk::Barrier& prologue,
        release_map_t& releases
    );

    // jvk waits: they join the jvk extra waits of the stage
    [[nodiscard]] cth::co::executor_task<std::vector<SemaphoreSubmitInfo>> release(
        release_map_t releases
    );

    /** the caller holds `_statesMutex`, after `append()` */
    void presentBarriers(dev::StageSlot&);

    /** throws only for a failed submit: nothing is presented or resolved then */
    [[nodiscard]] cth::co::executor_task<void> submitAndPresent(
        dev::StageSlot&,
        uint64_t frameNumber
    );

    /** the caller holds `_statesMutex` */
    void eraseStates(VkImage, uint32_t mipLevels);

    /** the caller holds `_statesMutex` */
    void eraseStates(VkBuffer);

    static void endPrimary(PrimaryCmdBuffer&);

    cth::ref_wrapper<Core> _core;
    Config _config;

    std::deque<dev::FrameSlot> _frameSlots{};

    std::mutex _statesMutex{}; ///< locks `_states`, `_imageQueues` and `_bufferQueues`

    ResourceTracker _states{};

    last_queue_map_t<image_mip_t> _imageQueues{};

    last_queue_map_t<VkBuffer> _bufferQueues{};

    //TODO review the error stays: every nextFrame() rethrows it, the errors of the submissions are
    // fatal
    cth::co::catching_scope _submissions{};

    uint64_t _firstFrameNumber;

    uint64_t _frameNumber;

public:
    StageGraph(StageGraph const& other) = delete;
    StageGraph(StageGraph&& other) = delete;
    StageGraph& operator=(StageGraph const& other) = delete;
    StageGraph& operator=(StageGraph&& other) = delete;
};
}
