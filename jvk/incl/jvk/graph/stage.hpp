#pragma once
#include "jvk/base/queue.hpp"
#include "jvk/base/stasis.hpp"
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/base/queue/submit_info.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>


namespace jvk {
class StageGraph;
}

namespace jvk::dev {
//TODO review name: cvk::SemaphoreSubmitInfo with the jvk semaphore and a fixed value
//TODO review int64_t value: a wait for a frame before the first frame is < 1, next() keeps adding to it
/**
 * semaphore of a prebuilt submit
 * @details converts to `cvk::SemaphoreSubmitInfo`, a `value < 0` waits for `0`
 */
struct SemaphoreSubmitInfo {
    cth::ref_wrapper<Semaphore const> semaphore; ///< the jvk semaphore: `Semaphore::submitted()` needs it

    /**
     * wait -> the stages that wait, signal -> the stages that finish before the signal
     */
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    /**
     * a frame number
     * @details `< 1` -> a frame before the first frame, its wait is reached at once
     */
    int64_t value;

    [[nodiscard]] constexpr operator cvk::SemaphoreSubmitInfo() const {
        return cvk::SemaphoreSubmitInfo{*semaphore, stages, static_cast<uint64_t>(std::max<int64_t>(value, 0))};
    }
};

//TODO review name: like cvk::SubmitInfo and the legacy jvk::SubmitInfo
/**
 * prebuilt submit of a stage slot
 * @details
 * - the graph builds it once, at its construction
 * - holds the values of the next submit or skip of the slot
 * - a skip submits the waits and the signal without the command buffers
 */
struct SubmitInfo {
    std::vector<cth::ref_wrapper<PrimaryCmdBuffer const>> cmdBuffers{}; ///< `StageSlot::prologue`, `StageSlot::primary`

    /**
     * one per `StageConfig::waits`: `StageSlot::semaphore` of the waited stage slot
     * @details the waited stage slot: stage `StageWait::stageId`, `StageWait::frameOffset` frame slots before
     * @details the waits for a stage on the same `Queue` come first, `SubmitInfo::queueWaits` of them
     */
    std::vector<SemaphoreSubmitInfo> waits{};

    //TODO review the waits on the same queue first, counted by queueWaits: like the legacy timeline waits first
    /**
     * the first `queueWaits` waits wait for a stage on the same `Queue`
     * @details the submit job awaits their `Semaphore::submitted()` before its submit
     */
    size_t queueWaits = 0;

    SemaphoreSubmitInfo signal; ///< `StageSlot::semaphore` with the frame number

    /**
     * advances every wait and signal value to the next frame of the slot
     * @details never reads the waited semaphores: their submit jobs run at the same time, a read can see an old frame
     * @param frameSlots `StageGraphConfig::frameSlots`, the values rise by it
     */
    constexpr void next(size_t frameSlots);
};

/**
 * resources of a stage in one frame slot
 * @details
 * - owned by the graph, live as long as the graph
 * - the `Stage` of each frame uses them
 */
struct StageSlot {
    cth::ref_wrapper<StageGraph> graph;

    size_t stageId; ///< id of the stage, see `StageGraphConfig::stages`

    size_t slot;
    cth::ref_wrapper<Queue const> queue;

    /**
     * signaled by the submit or the skip of each frame of the slot
     * @details value: the frame number, rises by `StageGraphConfig::frameSlots` per signal
     */
    Semaphore semaphore;

    //TODO review a member of the stage slot: it refers to StageSlot::semaphore, StageSlot::prologue,
    // StageSlot::primary and the semaphores of the waited stage slots, they live as long as the graph
    SubmitInfo submitInfo; ///< the waits and the signal of the next submit or skip, see `SubmitInfo::next()`

    cth::ref_wrapper<StageSlot const> previous; ///< the stage in the frame slot before, submits first

    PrimaryCmdBuffer prologue; ///< barriers for the first uses of the primary, submitted before it

    PrimaryCmdBuffer primary;

    std::vector<SecondaryCmdBuffer> secondaries{}; ///< `StageConfig::secondaries` of them

    std::mutex stasisMutex{};

    Stasis stasis; ///< destroyed when `StageSlot::semaphore` reaches the frame number of the next submit or skip
};
}

namespace jvk {
/**
 * submission of one stage in one frame
 * @details
 * - per frame: constructed by `StageGraph::nextFrame()`, handed out by `Frame::stage()`
 * - the resources come from its stage slot
 * - valid until `StageGraph::nextFrame()` hands out its slot again
 */
class Stage {
public:
    using Inheritance = SecondaryCmdBuffer::Inheritance;

    /**
     * what the stage did in its frame
     */
    enum class Status {
        UNSUBMITTED,
        SUBMITTED,
        SKIPPED,
    };

    /**
     * @param stageSlot resources of the stage in the frame slot
     */
    Stage(dev::StageSlot& stageSlot, uint64_t frameNumber);

    /**
     * begins the primary command buffer of the frame
     * @details resets the primary at each begin
     * @return the primary
     * @pre `status()` is `UNSUBMITTED`
     * @attention not thread safe
     */
    PrimaryCmdBuffer& beginPrimary();

    /**
     * primary command buffer of the frame slot
     * @details not begun
     * @see `beginPrimary()`
     */
    [[nodiscard]] PrimaryCmdBuffer& primary() { return _stageSlot->primary; }

    //TODO review std::optional instead of a default inheritance {}: an empty inheritance continues a rendering
    /**
     * begins the secondary command buffer of the frame slot
     * @param inheritanceOpt `std::nullopt` -> for use outside of a rendering
     * @return the secondary
     * @pre the preconditions of `secondary()`
     * @attention not thread safe for one `secondaryIdx`
     */
    SecondaryCmdBuffer& beginSecondary(
        size_t secondaryIdx,
        std::optional<Inheritance> const& inheritanceOpt = std::nullopt
    );

    /**
     * secondary command buffer of the frame slot
     * @details not begun
     * @details the same index in the next frames of the slot is the same secondary
     * @pre `secondaryIdx < StageConfig::secondaries`
     * @pre `primary()` executes it before the `submit()`
     * @see `beginSecondary()`
     */
    [[nodiscard]] SecondaryCmdBuffer& secondary(size_t secondaryIdx);

    /**
     * secondary command buffer for the queue family of the stage, owned by the caller
     * @details not begun
     * @throws cvk::vk_result_exception result of `vkCreateCommandPool()` or `vkAllocateCommandBuffers()`
     * @see `SecondaryCmdBuffer::begin()`
     */
    [[nodiscard]] SecondaryCmdBuffer persistentSecondary() const;

    /**
     * adds a semaphore wait to the submit of this frame
     * @details `submit()` and `skip()` wait for it
     * @pre `status()` is `UNSUBMITTED`
     * @pre the semaphore lives until the gpu finished the submit
     * @attention not thread safe
     */
    void addWait(cvk::SemaphoreSubmitInfo const&);

    /**
     * submits the primary of the frame
     * @details calls `StageGraph::submit()` of the graph of the stage
     * @pre the preconditions of `StageGraph::submit()`
     */
    void submit();

    /**
     * submits no work in this frame
     * @details calls `StageGraph::skip()` of the graph of the stage
     * @pre the preconditions of `StageGraph::skip()`
     */
    void skip();

    /**
     * keeps the object until the gpu finished the submit or the skip of this frame
     * @details thread safe
     * @pre `status()` is `UNSUBMITTED`
     * @pre its users on other queues finish before this submit: this submit waits for them
     */
    template<class T>
    requires(!std::is_lvalue_reference_v<T>) && requires(T& object) { object.release().decompose(); }
    void retain(T&& object);

    /**
     * waits until the gpu finished the submit or the skip of this frame
     * @details resumes on the context of the caller
     * @pre `status()` is not `UNSUBMITTED`
     */
    [[nodiscard]] cth::co::executor_task<void> wait() const;

    /**
     * waits until the submit or the skip of this frame reached `vkQueueSubmit2()`
     * @details
     * - not until the gpu finished
     * - resumes on the context of the caller
     * - reached -> no suspend
     * - await it before a submit on `queue()` that waits for `semaphore()`
     * - `Semaphore::submitted()` of `semaphore()` with `frameNumber()`
     * @see `wait()`
     */
    [[nodiscard]] constexpr submitted_awaiter submitted() const {
        return _stageSlot->semaphore.submitted(_frameNumber);
    }

    [[nodiscard]] constexpr Status status() const { return _status; }
    [[nodiscard]] constexpr size_t id() const { return _stageSlot->stageId; }
    [[nodiscard]] constexpr size_t slot() const { return _stageSlot->slot; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_stageSlot->queue; }

    /**
     * @return semaphore of the stage slot
     * @details the submit or the skip of frame N signals N
     */
    [[nodiscard]] constexpr Semaphore const& semaphore() const { return _stageSlot->semaphore; }

    /**
     * @return number of the frame, the value of `semaphore()` for this frame
     */
    [[nodiscard]] constexpr uint64_t frameNumber() const { return _frameNumber; }

private:
    // the graph sets the status of the submission, there is no other access without a public setter
    friend class StageGraph;

    cth::ref_wrapper<dev::StageSlot> _stageSlot;
    uint64_t _frameNumber;
    Status _status = Status::UNSUBMITTED;

    std::vector<cvk::SemaphoreSubmitInfo> _extraWaits{}; ///< the waits of `addWait()`

public:
    Stage(Stage const& other) = delete;
    Stage(Stage&& other) noexcept = default;
    Stage& operator=(Stage const& other) = delete;
    Stage& operator=(Stage&& other) noexcept = default;
};
}

namespace jvk::dev {
constexpr void SubmitInfo::next(size_t frameSlots) {
    for(auto& wait : waits)
        wait.value += static_cast<int64_t>(frameSlots);
    signal.value += static_cast<int64_t>(frameSlots);
}
}
