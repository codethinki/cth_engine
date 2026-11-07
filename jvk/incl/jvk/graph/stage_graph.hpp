#pragma once
#include "frame.hpp"
#include "present_stage.hpp"
#include "stage.hpp"
#include "stage_graph_config.hpp"

#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/resource_states.hpp"

#include "cvk/render/sync/barrier.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/data/hash_map.hpp>
#include <cth/ptr/not_null.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
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
 * @details one timeline semaphore per stage: the submit of frame N signals N
 * @details the stages of a frame record at the same time, on any thread
 * @details the submits of a stage follow the frame order
 * @details the graph states resolve the first uses of each submit in its prologue
 */
class StageGraph {
public:
    using Config = StageGraphConfig;

    //TODO review no base constructor and create(): the graph has no wrap or release
    /**
     * creates the stages, the frames and the present stage
     * @pre @ref Core::created()
     * @pre the config passes @ref Config::debug_check()
     * @throws cvk::vk_result_exception result of a create of the semaphores, fences or command buffers
     */
    StageGraph(Core&, Config);

    /**
     * waits for the fences of every slot, destroys the stages and the frames
     * @pre every frame submitted or skipped its stages
     */
    ~StageGraph();

    //TODO review window extent parameter, one caller at a time
    /**
     * hands out the frame of the next slot
     * @details waits for the fences of the stages of the slot
     * @details stages of the current frame without @ref Stage::submit() or @ref Stage::skip(): skips them,
     *  warns if a waiting stage waited for that skip
     * @details recreate flag of the present stage -> recreates the swapchain, see @ref Frame::extentChanged()
     * @param windowExtent extent for the recreate of the swapchain, ignored without present
     * @return the frame of the slot
     * @throws cvk::vk_result_exception the first error of a submit or present job
     * @attention not thread safe
     */
    [[nodiscard]] cth::co::executor_task<Frame&> nextFrame(VkExtent2D windowExtent = {});

    //TODO review public for the stages instead of a friend
    /**
     * posts the submit job of the stage into the strand of its queue
     * @details @ref Stage::submit() calls it
     * @details the job waits for the marks of the waited stages off the strand, submits in the strand
     * @details after the fence: clears the stasis of the stage
     */
    void submit(Stage&);

    //TODO review public for the jobs instead of a friend
    /**
     * stores the first error of a job for @ref nextFrame()
     */
    void storeError(std::exception_ptr) noexcept;

    //TODO review public for the frames instead of a friend
    /**
     * @return nullptr -> no present
     */
    [[nodiscard]] dev::PresentStage* presentStage() { return _presentOpt.has_value() ? &*_presentOpt : nullptr; }

    [[nodiscard]] constexpr Core& core() const { return *_corePtr; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:

    /**
     * release submit of one queue and frame slot
     * @details releases the resources that a submit on another family acquires
     */
    struct ReleaseSubmit {
        PrimaryCmdBuffer cmd;

        /**
         * the acquiring submit waits for it
         */
        cvk::Semaphore released;
    };

    /**
     * submit job of the stage, see @ref submit()
     */
    [[nodiscard]] cth::co::executor_task<void> submitJob(Stage&);

    cth::not_null_ptr<Core> _corePtr;
    Config _config;

    /**
     * one per stage
     */
    std::deque<cvk::TimelineSemaphore> _timelines{};

    std::vector<std::unique_ptr<Frame>> _frames{};
    std::optional<dev::PresentStage> _presentOpt = std::nullopt;

    std::mutex _statesMutex;
    dev::GraphStates _states{};

    /**
     * per last queue: one release submit per frame slot
     */
    cth::dt::flat_hash_map<Queue const*, std::vector<ReleaseSubmit>> _releases{};

    std::mutex _errorMutex;
    std::exception_ptr _error = nullptr;

    /**
     * number of the last frame that @ref nextFrame() handed out
     */
    uint64_t _frame = 0;

public:
    StageGraph(StageGraph const& other) = delete;
    StageGraph(StageGraph&& other) = delete;
    StageGraph& operator=(StageGraph const& other) = delete;
    StageGraph& operator=(StageGraph&& other) = delete;
};
}
