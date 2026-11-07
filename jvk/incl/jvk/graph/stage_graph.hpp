#pragma once
#include "frame.hpp"
#include "stage.hpp"
#include "stage_graph_config.hpp"

#include "jvk/sync/resource_states.hpp"
#include "jvk/sync/semaphore.hpp"

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

namespace jvk::dev {
/**
 * resource states after all submits of a graph
 * @details the last queue per resource, for the queue family ownership transfers
 * @attention not thread safe: lock @ref GraphShared::statesMutex
 */
class GraphStates {
public:
    /**
     * appends the states of a submit on the queue
     * @param prologue gets the barriers for the first uses of the submit, the acquire halves included
     * @param releases gets the release halves per last queue of another family, see @ref Queue::release()
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

//TODO review name and split: the parts of the graph that Stage::submit() and Stage::skip() need, reachable
// through dev::StageSlot::sharedPtr (no friend, no public graph internals)
/**
 * parts of a graph that its submits share
 * @details owned by the graph, the stage slots point to it
 */
struct GraphShared {
    /**
     * locks @ref states
     */
    std::mutex statesMutex{};

    GraphStates states{};

    std::mutex errorMutex{};

    /**
     * the first error of a submit job, @ref StageGraph::nextFrame() rethrows it
     */
    std::exception_ptr errorPtr = nullptr;
};
}


namespace jvk {
/**
 * stages with their waits, once per frame, in frame slots
 * @details
 * - one @ref Semaphore per stage: the submit or the skip of frame N signals N
 * - the stages of a frame record at the same time, on any thread
 * - the submits of a stage follow the frame order
 * - the graph states resolve the first uses of each submit in its prologue
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

    [[nodiscard]] constexpr Core& core() const { return *_corePtr; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
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
     * see @ref dev::StageSlot::sharedPtr
     */
    dev::GraphShared _shared{};

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
