#pragma once
#include "frame.hpp"
#include "stage.hpp"
#include "stage_graph_config.hpp"

#include "jvk/base/stasis.hpp"
#include "jvk/sync/resource_states.hpp"
#include "jvk/utility/first_error.hpp"

#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cth/coro/simple_counting_scope.hpp>
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

    //TODO review no base constructor and create(): the graph has no wrap or release
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
     * - the skip of an unsubmitted stage drops its binary waits: their semaphores, e.g. the acquire semaphores of a
     *   `Presenter`, can be destroyed before the graph
     * - waits for the end of every submit job
     * - a failed skip or wait does not stop the destruction
     * @attention blocks the calling thread: a worker of the core must be free for the jobs
     */
    ~StageGraph();

    //TODO review awaits Stage::submitted() of the old frame: the gpu can finish before the graph job marks the submit
    /**
     * constructs the frame and its stages in the next slot
     * @details the frame before: skips its stages without `Stage::submit()` or `Stage::skip()`, warns for a waited one
     * @details the old frame of the slot: awaits `Stage::submitted()` of its stages, then each stage: the gpu finished
     *  it
     * @return the frame of the slot
     * @pre no stage of the frame before records or submits: `nextFrame()` ends the frame
     * @throws cvk::vk_result_exception the first error of a submit job or a scope, stored before the call: an error
     *  during the waits -> the next `nextFrame()` rethrows it
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
     * - an `EXCLUSIVE` resource of another queue family: `Queue::release()` on its last queue, the prologue acquires it
     * - waits for the stage waits of the config and the waits of `Stage::await()`
     * - signals `Stage::semaphore()` with `Stage::frameNumber()`
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
     * @details empty submit on the queue of the stage:
     * - waits for the stage waits of the config and the waits of `Stage::await()`
     * - signals `Stage::semaphore()` with `Stage::frameNumber()`
     * @details after the gpu finished the empty submit: destroys the retained objects
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
     * @see `ResourceStates::assume()`
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
    using last_queue_map_t = cth::dt::flat_hash_map<Key, cth::ref_wrapper<Queue const>>;

    using release_map_t = cth::dt::flat_hash_map<Queue const*, cvk::Barrier>;

    //TODO review name
    using semaphore_waits_t = std::vector<cvk::SemaphoreSubmitInfo>;

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
     * - a failed allocation: the stage stays `UNSUBMITTED` without its waits and retained resources, the exception
     *   destroys the graph
     * @param targetStatus `SUBMITTED` or `SKIPPED`
     */
    void detachJob(Stage&, Stage::Status targetStatus);

    //TODO review no marks as parameters: the job awaits Semaphore::submitted() of the slots, they live as long as
    // the graph
    //TODO review next() before the mark: the job of the next frame of the slot awaits the mark, it reads the values
    //TODO review the gpu wait and the destruction in the strand of the queue: no extra post, the destruction is short
    /**
     * submit job of `submit()` and `skip()`
     * @details the order:
     * - off the strand: awaits `Semaphore::submitted()` of the stage in the frame before, of the waits of
     *   `StageSlot::submitInfo` and of every jvk extra wait: the appends follow the dependency order
     * - the binary extra waits: no await on the cpu
     * - `SUBMITTED`: `append()` of the states of the primary under `StageGraph::_statesMutex`, `release()` of the
     *   release halves, records the prologue, an empty prologue: no record, the submit without it
     * - in the strand: the submit of `SubmitInfo::vkSubmit()` with the release waits
     * - `SKIPPED` or a failed submit: in the strand, the submit of `SubmitInfo::vkSkip()`
     * - `SubmitInfo::next()`, then `Semaphore::markSubmitted()` of the slot with the frame number
     * - after the gpu finished: erases the states of the retained resources, then `Stasis::clear()` to `stasisEnd`
     * @details the errors:
     * - a failed submit: stores the error, see `dev::first_error`, then skips
     * - a failed skip: `VK_ERROR_DEVICE_LOST` -> stores the error, else terminates: the waiters would hang
     * - a failed gpu wait: stores the error, keeps the retained objects for the destruction of the graph
     * @param targetStatus the status the job reaches: `SUBMITTED` or `SKIPPED`
     * @param stasisEnd `Stasis::end()` of the stage slot at the submit or the skip
     * @see `Stage::submitted()`
     */
    [[nodiscard]] cth::co::executor_task<void> submitJob(
        dev::StageSlot&,
        uint64_t frameNumber,
        std::vector<SemaphoreSubmitInfo> extraWaits,
        semaphore_waits_t extraBinaryWaits,
        dev::RetainedResources retained,
        Stasis::pos stasisEnd,
        Stage::Status targetStatus
    );

    /**
     * appends the states of a submit on the queue to the resource states after all submits
     * @details the first use of a mip level or a buffer after the last state:
     * - unknown image: from `VK_IMAGE_LAYOUT_UNDEFINED`
     * - unknown buffer: no barrier
     * - the same queue family, or `VK_SHARING_MODE_CONCURRENT`: the barrier of `ResourceStates::append()`
     * - another queue family, `EXCLUSIVE`, contents needed: release half for the last queue, acquire half
     * - another queue family, discard or `VK_IMAGE_LAYOUT_UNDEFINED`: from `VK_IMAGE_LAYOUT_UNDEFINED`, no transfer
     * - an `assume()` before the first use: no barrier
     * @details sets the queue as the last queue of the resources
     * @param[out] prologue gets the barriers for the first uses of the submit, the acquire halves included
     * @param[out] releases gets the release halves per last queue of another family
     * @pre the caller holds `StageGraph::_statesMutex`
     */
    void append(ResourceStates const& recording, Queue const& queue, cvk::Barrier& prologue, release_map_t& releases);

    //TODO review jvk waits: they join the jvk extra waits of the stage
    /**
     * releases the resources on their last queues
     * @param releases the release halves per last queue, from `append()`
     * @return the waits for the releases
     * @see `Queue::release()`
     */
    [[nodiscard]] cth::co::executor_task<std::vector<SemaphoreSubmitInfo>> release(release_map_t releases);

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

    ResourceStates _states{}; ///< resource states after all submits

    /**
     * last queue per mip level of an image, for the queue family ownership transfers
     */
    last_queue_map_t<image_mip_t> _imageQueues{};

    last_queue_map_t<VkBuffer> _bufferQueues{}; ///< last queue per buffer, for the queue family ownership transfers

    //TODO review the error stays: every nextFrame() rethrows it, the errors of the jobs are fatal
    /**
     * the first error of a submit job or a scope of a stage, `nextFrame()` rethrows it
     */
    dev::first_error _error{};

    cth::co::simple_counting_scope _jobs{}; ///< the running submit jobs, `~StageGraph()` waits for them

    //TODO review name firstFrameNumber
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
