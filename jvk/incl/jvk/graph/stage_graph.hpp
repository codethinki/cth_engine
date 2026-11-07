#pragma once
#include "frame.hpp"
#include "stage.hpp"
#include "stage_graph_config.hpp"

#include "jvk/cmd/cmd_buffer.hpp"
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
#include <span>
#include <utility>
#include <vector>


namespace jvk {
class Core;
class Queue;
}

//TODO review dev wrapper instead of a queue field in ResourceStates
namespace jvk::dev {
/**
 * resource states after all submits of a graph
 * @details the last queue per resource, for the queue family ownership transfers
 * @attention not thread safe: the graph locks it
 */
class GraphStates {
public:
    /**
     * appends the states of a submit on the queue
     * @param prologue gets the barriers for the first uses of the submit, the acquire barriers included
     * @param releases gets the release barriers per last queue of another family
     */
    void append(
        ResourceStates const& recording,
        Queue const& queue,
        cvk::Barrier& prologue,
        cth::dt::flat_hash_map<Queue const*, cvk::Barrier>& releases
    );

private:
    ResourceStates _states{};
    cth::dt::flat_hash_map<std::pair<VkImage, uint32_t>, cth::not_null_ptr<Queue const>> _imageQueues{};
    cth::dt::flat_hash_map<VkBuffer, cth::not_null_ptr<Queue const>> _bufferQueues{};
};
}


namespace jvk {
/**
 * stages with their waits, once per frame, in frame slots
 * @details one @ref Semaphore per stage: the submit or the skip of frame N signals N
 * @details the stages of a frame record at the same time, on any thread
 * @details the submits of a stage follow the frame order
 * @details the graph states resolve the first uses of each submit in its prologue
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
     * @details awaits the semaphores of the stages for the last frame of the slot
     * @details stages of the current frame without @ref Stage::submit() or @ref Stage::skip(): skips them,
     *  warns if a waiting stage waited for that skip
     * @return the frame of the slot
     * @throws cvk::vk_result_exception the first error of a submit job
     * @attention not thread safe
     */
    [[nodiscard]] cth::co::executor_task<Frame&> nextFrame();

    //TODO review the prologue buffer: the graph does not know when this submit completes (no own signal),
    // options: a jvk::Semaphore per queue that every submit signals, a prologue buffer from the caller
    //TODO review ends the command buffer like Stage::submit()
    /**
     * submits the command buffer after the states of the graph
     * @details resolves the first uses of the command buffer against the states of the graph, under the lock
     * @details returns at once, the strand of the queue submits the prologue and the command buffer
     * @details copies the waits and the signals
     * @details thread safe
     * @param waits of the submit
     * @param signals of the submit
     * @pre the command buffer is begun, of the queue family of the queue, without an open rendering
     * @pre the submits that signal the timeline waits are submitted, see @ref Stage::submitted()
     * @pre the command buffer and the semaphores live until the gpu finished the submit
     * @note ends the command buffer
     */
    void submit(
        Queue const&,
        PrimaryCmdBuffer&,
        std::span<cvk::SemaphoreSubmitInfo const> waits,
        std::span<cvk::SemaphoreSubmitInfo const> signals
    );

    //TODO review only Stage::submit() can call it: the graph cannot set the status of the stage (private, no friend),
    // a direct call leaves the stage UNSUBMITTED
    /**
     * posts the submit job of the stage
     * @details called by @ref Stage::submit(), see there
     * @details the job awaits the marks of the waited stages off the strand, then submits like the other @ref submit()
     * @details adds the stage waits of the config and the extra waits of the stage, signals the semaphore of the stage
     * @details after the gpu finished the submit: clears the stasis of the stage
     */
    void submit(Stage&);

    [[nodiscard]] constexpr Core& core() const { return *_corePtr; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    //TODO review values of the release semaphore: the frame number fits one release submit per queue and frame
    /**
     * release submit of one queue and frame slot
     * @details releases the resources that a submit on another family acquires
     */
    struct ReleaseSubmit {
        PrimaryCmdBuffer cmd;

        /**
         * signaled by the release submit, the acquiring submit waits for it
         */
        Semaphore released;
    };

    /**
     * submit job of the stage, see @ref submit()
     */
    [[nodiscard]] cth::co::executor_task<void> submitJob(Stage&);

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

    std::mutex _statesMutex;
    dev::GraphStates _states{};

    /**
     * per last queue: one release submit per frame slot
     */
    cth::dt::flat_hash_map<Queue const*, std::vector<ReleaseSubmit>> _releases{};

    std::mutex _errorMutex;
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
