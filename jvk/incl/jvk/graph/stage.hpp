#pragma once
#include "jvk/base/queue.hpp"
#include "jvk/base/stasis.hpp"
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/base/queue/submit_info.hpp"

#include <volk.h>

#include <cth/coro/tasks/sync_task.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>


namespace cvk {
class Semaphore;
}

namespace jvk {
class StageGraph;
}

namespace jvk::dev {
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
     * @details value: a frame number, `< StageGraph::firstFrameNumber()` -> a frame before the first frame, reached
     *  from the start
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

//TODO review a constructor, no aggregate: `SubmitInfo::signal` and `SubmitInfo::cmdBuffers` refer to members, the
// mutex does not move
/**
 * resources of a stage in one frame slot
 * @details
 * - owned by the graph, live as long as the graph
 * - the `Stage` of each frame uses them
 */
struct StageSlot {
    /**
     * creates the semaphore and the command buffers of the stage slot
     * @details
     * - the semaphore starts at `StageGraph::firstFrameNumber() - 1`
     * - `SubmitInfo::signal`: the semaphore with the first frame number of the slot
     * - the graph sets `StageSlot::previous` and the waits of `StageSlot::submitInfo` once all slots exist
     * - `secondaries`: `StageConfig::secondaries` of the stage
     * @pre `StageGraph::firstFrameNumber()` is set
     * @throws cvk::vk_result_exception result of a create of the semaphore or the command buffers
     */
    StageSlot(StageGraph& graph, size_t stageId, size_t slot, Queue const& queue, size_t secondaries);

    cth::ref_wrapper<StageGraph> graph;

    size_t stageId; ///< id of the stage, see `StageGraphConfig::stages`

    size_t slot;
    cth::ref_wrapper<Queue const> queue;

    /**
     * signaled by the submit or the skip of each frame of the slot
     * @details value: the frame number, rises by `StageGraphConfig::frameSlots` per signal
     * @details initial value: `StageGraph::firstFrameNumber() - 1`
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

    //TODO review the job as a started sync_task: cth has no detached task, the next frame of the slot and the graph
    // destructor wait for it
    /**
     * the submit job of the last submit or skip, started
     * @details `StageGraph::nextFrame()` waits for it before the slot gets its next frame
     */
    std::optional<cth::co::sync_task<void>> jobOpt = std::nullopt;

public:
    StageSlot(StageSlot const& other) = delete;
    StageSlot(StageSlot&& other) = delete;
    StageSlot& operator=(StageSlot const& other) = delete;
    StageSlot& operator=(StageSlot&& other) = delete;
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
     * adds a wait on a jvk semaphore to the submit of this frame
     * @details `submit()` and `skip()` wait for it, the submit job first awaits its `Semaphore::submitted()`
     * @pre `status()` is `UNSUBMITTED`
     * @pre the semaphore lives until the gpu finished the submit
     * @attention not thread safe
     * @see `StageGraph::submit()`
     */
    void await(SemaphoreSubmitInfo const&);

    /**
     * adds a wait on a jvk semaphore to the submit of this frame
     * @details calls @ref await(SemaphoreSubmitInfo const&)
     * @param semaphore to wait for
     * @param stages of this submit that wait
     * @param value to wait for
     */
    void await(Semaphore const& semaphore, VkPipelineStageFlags2 stages, uint64_t value) {
        await(SemaphoreSubmitInfo{semaphore, stages, value});
    }

    //TODO review deleted with a reason (C++26): without it a jvk semaphore takes the binary overload through the
    // private base of cvk::TimelineSemaphore and fails with an inaccessible base error
    /**
     * a jvk semaphore needs a value
     */
    void await(Semaphore const&, VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) = delete(
        "a jvk semaphore needs a value: await(semaphore, stages, value)"
    );

    /**
     * adds a wait on a binary semaphore to the submit of this frame
     * @details `submit()` and `skip()` wait for it, no await on the cpu: for binary semaphores like the acquire
     * @param binary semaphore to wait for
     * @param stages of this submit that wait
     * @pre `status()` is `UNSUBMITTED`
     * @pre the signal of the semaphore is submitted before the submit of this frame
     * @pre the semaphore lives until the gpu finished the submit
     * @attention not thread safe
     */
    void await(cvk::Semaphore const& binary, VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

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
     * waits until the gpu finished the submit or the skip of this frame: `co_await stage`
     * @details
     * - resumes on the context of the caller
     * - reached -> no suspend
     * - `Semaphore::wait()` of `semaphore()` with `frameNumber()`
     * @pre `status()` is not `UNSUBMITTED`
     */
    [[nodiscard]] constexpr semaphore_awaiter operator co_await() const {
        return _stageSlot->semaphore.wait(_frameNumber);
    }

    /**
     * waits until the submit or the skip of this frame reached `vkQueueSubmit2()`
     * @details
     * - not until the gpu finished
     * - resumes on the context of the caller
     * - reached -> no suspend
     * - await it before a submit on `queue()` that waits for `semaphore()`
     * - `Semaphore::submitted()` of `semaphore()` with `frameNumber()`
     * @see `operator co_await()`
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
     * @details the first frame is `StageGraph::firstFrameNumber()`
     */
    [[nodiscard]] constexpr uint64_t frameNumber() const { return _frameNumber; }

private:
    // the graph sets the status of the submission, there is no other access without a public setter
    friend class StageGraph;

    cth::ref_wrapper<dev::StageSlot> _stageSlot;
    uint64_t _frameNumber;
    Status _status = Status::UNSUBMITTED;

    //TODO review names: the extra waits per kind, the jvk ones keep the jvk semaphore
    std::vector<SemaphoreSubmitInfo> _extraWaits{}; ///< the waits of @ref await(SemaphoreSubmitInfo const&)

    /**
     * the waits of @ref await(cvk::Semaphore const&, VkPipelineStageFlags2)
     */
    std::vector<cvk::SemaphoreSubmitInfo> _extraBinaryWaits{};

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
        wait.value += frameSlots;
    signal.value += frameSlots;
}
}
