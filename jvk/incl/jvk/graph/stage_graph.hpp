#pragma once
#include "frame.hpp"
#include "stage.hpp"
#include "stage_graph_config.hpp"

#include "jvk/sync/resource_states.hpp"
#include "jvk/sync/semaphore.hpp"

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
#include <optional>
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
 * - one @ref Semaphore per stage: the submit or the skip of frame N signals N
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
     * @pre @ref Core::created()
     * @pre the config passes @ref Config::debug_check()
     * @throws cvk::vk_result_exception result of a create of the semaphores or command buffers
     */
    StageGraph(Core&, Config);

    /**
     * waits until the gpu finished the submits of every slot, destroys the slots
     * @pre every frame submitted or skipped its stages
     */
    ~StageGraph();

    /**
     * constructs the frame and its stages in the next slot
     * @details
     * - awaits the semaphores of the stages for the last frame of the slot
     * - a stage of the current frame without @ref Stage::submit() or @ref Stage::skip(): skips it
     * - a stage that waited for such a skip: warns
     * @return the frame of the slot
     * @throws cvk::vk_result_exception the first error of a submit job
     * @attention not thread safe
     */
    [[nodiscard]] cth::co::executor_task<Frame&> nextFrame();

    /**
     * submits the primary of the stage
     * @details
     * - sets the status of the stage to SUBMITTED
     * - thread safe for different stages
     * - returns at once, the strand of the queue submits after the waited stages submitted
     * - resolves the first uses of the primary against the resource states after all submits
     * - waits for the stage waits of the config and the waits of @ref Stage::addWait()
     * - signals @ref Stage::semaphore() with @ref Stage::frameNumber()
     * - after the gpu finished: destroys the retained objects
     * @pre the stage is of this graph
     * @pre @ref Stage::beginPrimary() in this frame
     * @pre @ref Stage::status() is UNSUBMITTED
     * @pre the primary has no open rendering
     * @pre the primary and its executed secondaries belong to the queue family of @ref Stage::queue()
     * @note ends the primary
     */
    void submit(Stage&);

    /**
     * submits no work for the stage in this frame
     * @details sets the status of the stage to SKIPPED, thread safe for different stages
     * @details returns at once, the strand of the queue submits after the waited stages submitted
     * @details empty submit on the queue of the stage:
     * - waits for the stage waits of the config and the waits of @ref Stage::addWait()
     * - signals @ref Stage::semaphore() with @ref Stage::frameNumber()
     * @details after the gpu finished the empty submit: destroys the retained objects
     * @pre the stage is of this graph
     * @pre no @ref Stage::beginPrimary() in this frame
     * @pre @ref Stage::status() is UNSUBMITTED
     */
    void skip(Stage&);

    [[nodiscard]] constexpr Core& core() const { return *_corePtr; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    /**
     * submit job of @ref submit() and @ref skip()
     * @details
     * - off the strand: awaits the marks of the previous frame and of the waited stages on the same queue
     * - under @ref _statesMutex: @ref append() of the states of the primary
     * - release halves: @ref release() on the last queues, the submit waits for the returned values
     * - the prologue records the acquire halves and the barriers of the first uses
     * - in the strand of the queue: the submit, then signals the mark
     * - after the gpu finished: clears the stasis
     * - stores its first error, see @ref storeError()
     * @details the status: SUBMITTED -> the primary, SKIPPED -> the empty submit
     */
    [[nodiscard]] cth::co::executor_task<void> submitJob(
        dev::StageSlot&,
        uint64_t frameNumber,
        std::vector<cvk::SemaphoreSubmitInfo> extraWaits,
        Stage::Status
    );

    /**
     * appends the states of a submit on the queue to the resource states after all submits
     * @details sets the queue as the last queue of the resources
     * @param prologue gets the barriers for the first uses of the submit, the acquire halves included
     * @param releases gets the release halves per last queue of another family
     * @pre the caller holds @ref _statesMutex
     */
    void append(
        ResourceStates const& recording,
        Queue const& queue,
        cvk::Barrier& prologue,
        cth::dt::flat_hash_map<Queue const*, cvk::Barrier>& releases
    );

    /**
     * releases the resources on their last queues, see @ref Queue::release()
     * @param releases the release halves per last queue, from @ref append()
     * @return the waits for the releases
     */
    [[nodiscard]] cth::co::executor_task<std::vector<cvk::SemaphoreSubmitInfo>> release(
        cth::dt::flat_hash_map<Queue const*, cvk::Barrier> releases
    );

    /**
     * stores the first error of a job for @ref nextFrame()
     */
    void storeError(std::exception_ptr) noexcept;

    cth::not_null_ptr<Core> _corePtr;
    Config _config;

    /**
     * one per stage, see @ref dev::StageSlot::semaphorePtr
     */
    std::deque<Semaphore> _semaphores{};

    /**
     * one per frame slot, with its stage slots
     */
    std::deque<dev::FrameSlot> _frameSlots{};

    /**
     * the current frame per frame slot, constructed by @ref nextFrame()
     */
    std::vector<std::optional<Frame>> _frames{};

    /**
     * locks @ref _states, @ref _imageQueues and @ref _bufferQueues
     */
    std::mutex _statesMutex{};

    /**
     * resource states after all submits
     */
    ResourceStates _states{};

    /**
     * last queue per mip level of an image, for the queue family ownership transfers
     */
    cth::dt::flat_hash_map<std::pair<VkImage, uint32_t>, cth::not_null_ptr<Queue const>> _imageQueues{};

    /**
     * last queue per buffer, for the queue family ownership transfers
     */
    cth::dt::flat_hash_map<VkBuffer, cth::not_null_ptr<Queue const>> _bufferQueues{};

    std::mutex _errorMutex{};

    /**
     * the first error of a submit job, @ref nextFrame() rethrows it
     */
    std::exception_ptr _errorPtr = nullptr;

    /**
     * number of the last frame that @ref nextFrame() handed out
     */
    uint64_t _frameNumber = 0;

public:
    StageGraph(StageGraph const& other) = delete;
    StageGraph(StageGraph&& other) = delete;
    StageGraph& operator=(StageGraph const& other) = delete;
    StageGraph& operator=(StageGraph&& other) = delete;
};
}
