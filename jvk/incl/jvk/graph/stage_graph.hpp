#pragma once
#include "frame.hpp"
#include "stage.hpp"
#include "stage_graph_config.hpp"

#include "jvk/sync/resource_states.hpp"

#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/data/hash_map.hpp>
#include <cth/ptr/not_null.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
#include <utility>
#include <vector>


namespace jvk {
class Core;
class Queue;
}

namespace jvk {
/**
 * stages with their waits, once per frame, in frame slots
 * @details
 * - one `Semaphore` per stage slot: the submit or the skip of frame N signals N
 * - the stages of a frame record at the same time, on any thread
 * - the submits of a stage follow the frame order
 * - the resource states after all submits resolve the first uses of each submit in its prologue
 */
class StageGraph {
public:
    using Config = StageGraphConfig;

    //TODO review no base constructor and create(): the graph has no wrap or release
    /**
     * creates the stage slots and the frame slots
     * @pre `Core::created()`
     * @pre the config passes `Config::debug_check()`
     * @throws cvk::vk_result_exception result of a create of the semaphores or command buffers
     */
    StageGraph(Core&, Config);

    /**
     * waits until the gpu finished the submits of every slot, destroys the slots
     * @pre every frame submitted or skipped its stages
     */
    ~StageGraph();

    //TODO review awaits Stage::submitted() of the old frame: the gpu can finish before the graph job sets the mark,
    // the mark is destroyed with the old stage
    /**
     * constructs the frame and its stages in the next slot
     * @details the old frame of the slot:
     * - a stage without `Stage::submit()` or `Stage::skip()`: skips it
     * - a stage that waited for such a skip: warns
     * - awaits `Stage::submitted()` and `Stage::semaphore()` of its stages
     * @return the frame of the slot
     * @throws cvk::vk_result_exception the first error of a submit job
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
     * - waits for the stage waits of the config and the waits of `Stage::addWait()`
     * - signals `Stage::semaphore()` with `Stage::frameNumber()`
     * - after the gpu finished: destroys the retained objects
     * @pre the stage is of this graph
     * @pre `Stage::beginPrimary()` in this frame
     * @pre `Stage::status()` is `UNSUBMITTED`
     * @pre the primary has no open rendering
     * @pre the primary and its executed secondaries belong to the queue family of `Stage::queue()`
     * @note ends the primary
     */
    void submit(Stage&);

    /**
     * submits no work for the stage in this frame
     * @details sets the status of the stage to `SKIPPED`, thread safe for different stages
     * @details returns at once, the strand of the queue submits after the waited stages submitted
     * @details empty submit on the queue of the stage:
     * - waits for the stage waits of the config and the waits of `Stage::addWait()`
     * - signals `Stage::semaphore()` with `Stage::frameNumber()`
     * @details after the gpu finished the empty submit: destroys the retained objects
     * @pre the stage is of this graph
     * @pre no `Stage::beginPrimary()` in this frame
     * @pre `Stage::status()` is `UNSUBMITTED`
     */
    void skip(Stage&);

    [[nodiscard]] constexpr Core& core() const { return *_corePtr; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    using image_mip_t = std::pair<VkImage, uint32_t>;

    template<class Key>
    using last_queue_map_t = cth::dt::flat_hash_map<Key, cth::not_null_ptr<Queue const>>;

    using release_map_t = cth::dt::flat_hash_map<Queue const*, cvk::Barrier>;

    //TODO review name
    using semaphore_waits_t = std::vector<cvk::SemaphoreSubmitInfo>;

    //TODO review the marks as parameters: submit() and skip() take them from the stages while those live, the job
    // must register on the marks of earlier frames before nextFrame() replaces their stages
    /**
     * submit job of `submit()` and `skip()`
     * @details
     * - off the strand: awaits the marks of the previous frame and of the waited stages on the same queue
     * - under `StageGraph::_statesMutex`: `append()` of the states of the primary
     * - release halves: `release()` on the last queues, the submit waits for the returned values
     * - the prologue records the acquire halves and the barriers of the first uses
     * - in the strand of the queue: the submit, then sets the mark, posts its callbacks to the workers
     * - after the gpu finished: clears the stasis
     * - an error -> stores it, see `storeError()`, then sets the mark
     * @details the status: `SUBMITTED` -> the primary, `SKIPPED` -> the empty submit
     * @param mark of the stage in this frame, see `Stage::submitted()`
     * @param markAwaiters of the marks to await off the strand
     */
    [[nodiscard]] cth::co::executor_task<void> submitJob(
        dev::StageSlot&,
        uint64_t frameNumber,
        semaphore_waits_t extraWaits,
        Stage::Status,
        dev::SubmitMark& mark,
        std::vector<submitted_awaiter> markAwaiters
    );

    /**
     * appends the states of a submit on the queue to the resource states after all submits
     * @details sets the queue as the last queue of the resources
     * @param prologue gets the barriers for the first uses of the submit, the acquire halves included
     * @param releases gets the release halves per last queue of another family
     * @pre the caller holds `StageGraph::_statesMutex`
     */
    void append(ResourceStates const& recording, Queue const& queue, cvk::Barrier& prologue, release_map_t& releases);

    /**
     * releases the resources on their last queues, see `Queue::release()`
     * @param releases the release halves per last queue, from `append()`
     * @return the waits for the releases
     */
    [[nodiscard]] cth::co::executor_task<semaphore_waits_t> release(release_map_t releases);

    /**
     * stores the first error of a job for `nextFrame()`
     */
    void storeError(std::exception_ptr) noexcept;

    cth::not_null_ptr<Core> _corePtr;
    Config _config;

    /**
     * one per frame slot, with its stage slots
     */
    std::deque<dev::FrameSlot> _frameSlots{};

    /**
     * locks `StageGraph::_states`, `StageGraph::_imageQueues` and `StageGraph::_bufferQueues`
     */
    std::mutex _statesMutex{};

    /**
     * resource states after all submits
     */
    ResourceStates _states{};

    /**
     * last queue per mip level of an image, for the queue family ownership transfers
     */
    last_queue_map_t<image_mip_t> _imageQueues{};

    /**
     * last queue per buffer, for the queue family ownership transfers
     */
    last_queue_map_t<VkBuffer> _bufferQueues{};

    std::mutex _errorMutex{};

    /**
     * the first error of a submit job, `nextFrame()` rethrows it
     */
    std::exception_ptr _errorPtr = nullptr;

    /**
     * number of the last frame that `nextFrame()` handed out
     */
    uint64_t _frameNumber = 0;

public:
    StageGraph(StageGraph const& other) = delete;
    StageGraph(StageGraph&& other) = delete;
    StageGraph& operator=(StageGraph const& other) = delete;
    StageGraph& operator=(StageGraph&& other) = delete;
};
}
