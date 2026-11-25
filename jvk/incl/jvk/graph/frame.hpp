#pragma once
#include "stage.hpp"
#include "surface.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>


namespace jvk {
class StageGraph;
}

namespace jvk::dev {
struct FrameSlot;
}

namespace jvk {
/**
 * one frame
 * @details
 * - per frame: constructed by `StageGraph::nextFrame()`
 * - the resources come from its frame slot
 * - valid until `StageGraph::nextFrame()` hands out its slot again
 */
class Frame {
public:
    /**
     * @param frameSlot resources of the frame slot, with the stages of the frame
     */
    Frame(dev::FrameSlot& frameSlot, uint64_t number);

    /**
     * @return the stage in this frame
     * @pre `stageId < StageGraphConfig::stages.size()`
     */
    [[nodiscard]] Stage& stage(size_t stageId);

    /**
     * @return the stage in this frame, see `stage()`
     */
    [[nodiscard]] Stage& operator[](size_t stageId) { return stage(stageId); }

    /**
     * acquires the next image of the surface for a stage that waits for it and a stage that
     * presents it
     * @details the image, see `Surface::acquire()`
     * @details the acquire stage:
     * - waits for `Presenter::AcquiredImage::wait()` with the stages, see `Stage::await()`: the
     *   acquire semaphore or the ready point of a dropped image
     * - the state of the image in the graph: `VK_IMAGE_LAYOUT_UNDEFINED` with the wait stages, see
     *   `StageGraph::assume()`: the first barrier of the image chains with the wait
     * @details the present stage holds the image until its submission presents or drops it, see
     *  `Stage::present()`
     * @param surface of the image
     * @param acquireStage waits for the acquire, the first use of the image
     * @param presentStage presents the image after its submit
     * @param stages pipeline stages of the acquire stage that wait for the acquire
     * @return view of the image, `std::nullopt` -> surface extent `0`: no image, no wait, no
     *  present
     * @pre `Stage::status()` of both stages is `UNSUBMITTED`
     * @pre both stages belong to this frame
     * @pre the present stage waits for every stage of the frame that uses the image, the acquire
     *  stage included, directly or through other stages, or is that stage, see `StageWait`
     * @pre the preconditions of `Surface::acquire()`
     * @throws std::exception the exceptions of `Surface::acquire()`
     */
    [[nodiscard]] cth::co::executor_task<std::optional<SwapchainImage>> acquire(
        Surface& surface,
        Stage& acquireStage,
        Stage& presentStage,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    );

    /**
     * acquires the next image of the surface for one stage that uses and presents it
     * @details calls @ref acquire(Surface&, Stage&, Stage&, VkPipelineStageFlags2) with the stage
     *  for both
     */
    [[nodiscard]] cth::co::executor_task<std::optional<SwapchainImage>> acquire(
        Surface& surface,
        Stage& stage,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    ) {
        return acquire(surface, stage, stage, stages);
    }

    /**
     * @return number of the frame, the first frame is `StageGraph::firstFrameNumber()`
     */
    [[nodiscard]] constexpr uint64_t number() const { return _number; }

    [[nodiscard]] constexpr size_t slot() const;

private:
    cth::ref_wrapper<dev::FrameSlot> _frameSlot;
    uint64_t _number;

public:
    Frame(Frame const& other) = delete;
    Frame(Frame&& other) noexcept = default;
    Frame& operator=(Frame const& other) = delete;
    Frame& operator=(Frame&& other) noexcept = default;
};
}

namespace jvk::dev {
/**
 * resources of one frame slot
 * @details
 * - owned by the graph, live as long as the graph
 * - the `Frame` of each frame uses them
 */
struct FrameSlot {
    cth::ref_wrapper<StageGraph> graph;
    size_t slot;

    std::deque<StageSlot> stageSlots{}; ///< one per stage of the config

    /** the stages of the current frame, constructed by `StageGraph::nextFrame()` */
    std::vector<Stage> stages{};

    /**
     * the current frame of the slot, constructed by `StageGraph::nextFrame()`
     * @details `std::nullopt` -> the slot had no frame yet
     */
    std::optional<Frame> frameOpt = std::nullopt;
};
}

namespace jvk {
constexpr size_t Frame::slot() const { return _frameSlot->slot; }
}
