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
     * - waits for the end of every submit job
     * - a failed skip or wait does not stop the destruction
     * @attention blocks the calling thread: a worker of the core must be free for the jobs
     */
    ~StageGraph();

    // awaits Stage::submitted() of the old frame: the gpu can finish before the graph job marks the
    // submit
    /**
     * constructs the frame and its stages in the next slot
     * @details the frame before: skips its stages without `Stage::submit()` or `Stage::skip()`, warns for a waited one
     * @details the old frame of the slot: awaits `StageSlot::submitDone` of its stages: their jobs
     *  ended the submits and the presents, then the stages: the gpu finished them, both with
     *  `cth::co::all()`
     * @return the frame of the slot
     * @pre no stage of the frame before records or submits: `nextFrame()` ends the frame
     * @throws std::exception the first error of a submit job or a scope, stored before the call: an
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
     * @pre the jobs of the submits before that use the image appended their states: they are marked
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

    /**
     * skips the stages of the frame in the slot without `Stage::submit()` or `Stage::skip()`
     * @details a skipped stage that other stages wait for: warns
     * @details no frame in the slot -> nothing
     */
    void skipUnsubmitted(dev::FrameSlot&);

    //TODO review one helper for submit() and skip(): they differ in the status only
    /**
     * detaches the submit job, then sets the status of the stage
     * @details
     * - takes `Stasis::end()` of the stage slot and the retained resources of the stage under
     *   `StageSlot::stasisMutex`
     * - a failed allocation: the stage stays `UNSUBMITTED` without its retained resources, the exception destroys
     *   the graph
     * @param targetStatus `SUBMITTED` or `SKIPPED`
     */
    void detachJob(Stage&, Stage::Status targetStatus);

    // no marks as parameters: the job awaits Semaphore::submitted() of the slots, they live as
    // long as the graph
    // next() before the mark: the job of the next frame of the slot awaits the mark, it reads the
    // values
    /**
     * submit job of `submit()` and `skip()`
     * @details the order:
     * - off the strand: awaits `Semaphore::submitted()` of the stage in the frame before, of the
     *   waits of `StageSlot::slotSubmit` and of every jvk extra wait: the appends follow the
     *   dependency order
     * - the binary extra waits: no await on the cpu
     * - `SlotSubmit::skipInfo()` of the frame into `StageSlot::submitInfo`, clears the extra waits
     *   of the slot, then `SlotSubmit::next()`
     * - `SUBMITTED`: `append()` of the states of the primary under `StageGraph::_statesMutex`, `release()` of the
     *   release halves, records the prologue, an empty prologue: no record, the submit without it
     * - `SUBMITTED`: `presentBarriers()` under the same lock, records the epilogue, no presents: no
     *   record, the submit without it
     * - `SUBMITTED`: `submitAndPresent()` with the release waits, the submit marks the slot with
     *   the frame number
     * - `SKIPPED` or a failed submit: `dev::skip_submit()` of the skip info, then drops the
     *   presents after the gpu finished it
     * - then sets `StageSlot::submitDone`: the job ended its use of the slot before the gpu wait
     * - on the workers of the core: after the gpu finished, erases the states of the retained resources, then
     *   `Stasis::clear()` to `stasisEnd`
     * @details the errors:
     * - a failed submit: stores the error in `StageGraph::_jobs`, then skips
     * - a failed skip: `VK_ERROR_DEVICE_LOST` -> stores the error, marks the slot, else terminates:
     *   the waiters would hang
     * - before the mark: every other failure terminates, the waiters would hang
     * - after the mark: an escaped exception ends the job, `StageGraph::_jobs` stores it
     * - a failed gpu wait escapes: the retained objects stay for the destruction of the graph
     * @param targetStatus the status the job reaches: `SUBMITTED` or `SKIPPED`
     * @param stasisEnd `Stasis::end()` of the stage slot at the submit or the skip
     * @see `Stage::submitted()`
     */
    [[nodiscard]] cth::co::executor_task<void> submitJob(
        dev::StageSlot&,
        uint64_t frameNumber,
        dev::RetainedResources retained,
        Stasis::pos stasisEnd,
        Stage::Status targetStatus
    );

    /**
     * appends the states of a submit on the queue to the resource states after all submits
     * @details the first use of a mip level or a buffer after the last state:
     * - unknown image: from `VK_IMAGE_LAYOUT_UNDEFINED`
     * - unknown buffer: no barrier
     * - the same queue family, or `VK_SHARING_MODE_CONCURRENT`: the barrier of
     *   `ResourceTracker::append()`
     * - another queue family, `EXCLUSIVE`, contents needed: release half for the last queue, acquire half
     * - another queue family, discard or `VK_IMAGE_LAYOUT_UNDEFINED`: from `VK_IMAGE_LAYOUT_UNDEFINED`, no transfer
     * - an `assume()` before the first use: no barrier
     * @details sets the queue as the last queue of the resources
     * @param[out] prologue gets the barriers for the first uses of the submit, the acquire halves included
     * @param[out] releases gets the release halves per last queue of another family
     * @pre the caller holds `StageGraph::_statesMutex`
     */
    void append(
        ResourceTracker const& recording,
        Queue& queue,
        cvk::Barrier& prologue,
        release_map_t& releases
    );

    // jvk waits: they join the jvk extra waits of the stage
    /**
     * releases the resources on their last queues
     * @param releases the release halves per last queue, from `append()`
     * @return the waits for the releases
     * @see `Queue::transfer()`
     */
    [[nodiscard]] cth::co::executor_task<std::vector<SemaphoreSubmitInfo>> release(
        release_map_t releases
    );

    /**
     * fills `StageSlot::epilogueBarriers` with the barriers of the presented images
     * @details per image of `StageSlot::presents`, from its state after all submits:
     * - the queue of its surface has the family of the stage: to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`
     * - another family: the release half towards the family of the surface
     * @details stores the state before the barrier in `StageSlot::presentStates`, sets the state
     *  after it in `StageGraph::_states`
     * @pre the caller holds `StageGraph::_statesMutex`
     * @pre `append()` of the recording of the stage ran
     */
    void presentBarriers(dev::StageSlot&);

    /**
     * submits `StageSlot::submitInfo`, presents the images of `StageSlot::presents` after it
     * @details the images grouped by the queues of their surfaces:
     * - the queue of the stage: `Queue::present()` with the submit, one strand job
     * - another queue of the family: the submit signals their present semaphores, then
     *   `Queue::present()` without a submit on that queue
     * - another family: after the submit `Queue::transfer()` of the acquire halves on that queue,
     *   it signals the present semaphores, then the present, then the wait until the gpu finished
     *   the transfer: it waits for the semaphore of the slot
     * @details the end: each image resolves with its result, an image without a present (failed
     *  transfer) drops after the gpu finished the submit, then clears `StageSlot::presents`
     * @details a failed present or transfer: `StageGraph::_jobs` stores the error
     * @throws std::exception reason: the submit failed, nothing is presented or resolved
     */
    [[nodiscard]] cth::co::executor_task<void> submitAndPresent(
        dev::StageSlot&,
        uint64_t frameNumber
    );

    /**
     * removes the states and the last queues of the mip levels
     * @pre the caller holds `StageGraph::_statesMutex`
     */
    void eraseStates(VkImage, uint32_t mipLevels);

    /**
     * removes the state and the last queue of the buffer
     * @pre the caller holds `StageGraph::_statesMutex`
     */
    void eraseStates(VkBuffer);

    /**
     * ends an open rendering and the primary
     */
    static void endPrimary(PrimaryCmdBuffer&);

    cth::ref_wrapper<Core> _core;
    Config _config;

    std::deque<dev::FrameSlot> _frameSlots{}; ///< one per frame slot, with its stage slots

    /**
     * locks `StageGraph::_states`, `StageGraph::_imageQueues` and `StageGraph::_bufferQueues`
     */
    std::mutex _statesMutex{};

    ResourceTracker _states{}; ///< resource states after all submits

    /**
     * last queue per mip level of an image, for the queue family ownership transfers
     */
    last_queue_map_t<image_mip_t> _imageQueues{};

    /** last queue per buffer, for the queue family ownership transfers */
    last_queue_map_t<VkBuffer> _bufferQueues{};

    //TODO review the error stays: every nextFrame() rethrows it, the errors of the jobs are fatal
    /**
     * the running submit jobs, `~StageGraph()` waits for them
     * @details the first error of a job or a scope of a stage, `nextFrame()` rethrows it
     */
    cth::co::catching_scope _jobs{};

    uint64_t _firstFrameNumber; ///< see `firstFrameNumber()`

    /**
     * number of the last frame that `nextFrame()` handed out
     * @details `firstFrameNumber() - 1` before the first frame
     */
    uint64_t _frameNumber;

public:
    StageGraph(StageGraph const& other) = delete;
    StageGraph(StageGraph&& other) = delete;
    StageGraph& operator=(StageGraph const& other) = delete;
    StageGraph& operator=(StageGraph&& other) = delete;
};
}
