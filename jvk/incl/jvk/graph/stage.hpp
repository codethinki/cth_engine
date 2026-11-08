#pragma once
#include "jvk/base/queue.hpp"
#include "jvk/base/stasis.hpp"
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/base/queue/submit_info.hpp"

#include <volk.h>

#include <cth/coro/context.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/coro/utility/fwd.hpp>
#include <cth/ptr/not_null.hpp>

#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>


namespace jvk {
class StageGraph;
}

namespace jvk::dev {
struct StageSlot;

/**
 * wait of a stage slot for the slot of the waited stage, from a `StageWait`
 */
struct SlotWait {
    /**
     * the waited stage in the frame slot of the waited frame
     * @details the submit waits for its `StageSlot::semaphore` with the number of the waited frame
     */
    cth::not_null_ptr<StageSlot const> slotPtr;

    /**
     * see `StageWait::stages`
     */
    VkPipelineStageFlags2 stages;

    /**
     * see `StageWait::frameOffset`
     */
    uint32_t frameOffset;
};

/**
 * resources of a stage in one frame slot
 * @details
 * - owned by the graph, live as long as the graph
 * - the `Stage` of each frame uses them
 */
struct StageSlot {
    cth::not_null_ptr<StageGraph> graphPtr;

    /**
     * id of the stage, see `StageGraphConfig::stages`
     */
    size_t stageId;

    size_t slot;
    cth::not_null_ptr<Queue const> queuePtr;

    /**
     * signaled by the submit or the skip of each frame of the slot
     * @details value: the frame number, rises by `StageGraphConfig::frameSlots` per signal
     */
    Semaphore semaphore;

    /**
     * one per `StageConfig::waits`
     */
    std::vector<SlotWait> waits{};

    /**
     * the stage in the frame slot before, submits first
     */
    cth::not_null_ptr<StageSlot const> previousPtr;

    /**
     * barriers for the first uses of the primary, submitted before it
     */
    PrimaryCmdBuffer prologue;

    PrimaryCmdBuffer primary;

    /**
     * `StageConfig::secondaries` of them
     */
    std::vector<SecondaryCmdBuffer> secondaries{};

    std::mutex stasisMutex{};

    /**
     * destroyed when `StageSlot::semaphore` reaches the frame number of the next submit or skip
     */
    Stasis stasis;
};

/**
 * submitted mark of a stage in one frame
 * @details
 * - set once by the graph job, after the `vkQueueSubmit2()` of the submit or the skip
 * - the graph job posts the callbacks to the workers of the core
 */
struct SubmitMark {
    std::mutex mutex{};

    /**
     * the graph job reached `vkQueueSubmit2()`
     */
    bool submitted = false;

    /**
     * the waiters before `SubmitMark::submitted`, see `submitted_awaiter`
     */
    std::vector<cth::co::context::await_func> callbacks{};
};
}

namespace jvk {
//TODO review name: snake_case like semaphore_awaiter
/**
 * awaiter of `Stage::submitted()`
 * @details resumes the caller on its context
 */
class submitted_awaiter : public cth::co::this_coro_awaiter_base {
public:
    constexpr explicit submitted_awaiter(dev::SubmitMark& mark) : _markPtr{&mark} {}

    /**
     * @return the mark is set
     */
    [[nodiscard]] bool await_ready() const;

    /**
     * adds the resume of the caller to the callbacks of the mark
     * @details resumes on the context of the caller
     * @return `false` -> the mark is set in the meantime, the caller continues
     */
    template<class Caller>
    requires cth::co::this_coro_compatible<Caller>
    bool await_suspend(std::coroutine_handle<Caller> caller);

    //TODO review no error: the graph job sets the mark on every path, its error goes to StageGraph::nextFrame()
    constexpr void await_resume() const noexcept {}

private:
    cth::not_null_ptr<dev::SubmitMark> _markPtr;
};

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
     * @param slotPtr resources of the stage in the frame slot
     * @pre `slotPtr != nullptr`
     */
    Stage(dev::StageSlot* slotPtr, uint64_t frameNumber);

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
     * @details not begun, see `beginPrimary()`
     */
    [[nodiscard]] PrimaryCmdBuffer& primary() { return _slotPtr->primary; }

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
     * @details not begun, see `beginSecondary()`
     * @details the same index in the next frames of the slot is the same secondary
     * @pre `secondaryIdx < StageConfig::secondaries`
     * @pre `primary()` executes it before the `submit()`
     */
    [[nodiscard]] SecondaryCmdBuffer& secondary(size_t secondaryIdx);

    /**
     * secondary command buffer for the queue family of the stage, owned by the caller
     * @details not begun, see `SecondaryCmdBuffer::begin()`
     * @throws cvk::vk_result_exception result of `vkCreateCommandPool()` or `vkAllocateCommandBuffers()`
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
     * - not until the gpu finished, see `wait()`
     * - resumes on the context of the caller
     * - reached -> no suspend
     * - await it before a submit on `queue()` that waits for `semaphore()`
     */
    [[nodiscard]] constexpr submitted_awaiter submitted() const { return submitted_awaiter{*_submitMarkPtr}; }

    [[nodiscard]] constexpr Status status() const { return _status; }
    [[nodiscard]] constexpr size_t id() const { return _slotPtr->stageId; }
    [[nodiscard]] constexpr size_t slot() const { return _slotPtr->slot; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_slotPtr->queuePtr; }

    /**
     * @return semaphore of the stage slot
     * @details the submit or the skip of frame N signals N
     */
    [[nodiscard]] constexpr Semaphore const& semaphore() const { return _slotPtr->semaphore; }

    /**
     * @return number of the frame, the value of `semaphore()` for this frame
     */
    [[nodiscard]] constexpr uint64_t frameNumber() const { return _frameNumber; }

private:
    // the graph sets the status and the submitted mark of the submission, there is no other access without a
    // public setter
    friend class StageGraph;

    cth::not_null_ptr<dev::StageSlot> _slotPtr;
    uint64_t _frameNumber;
    Status _status = Status::UNSUBMITTED;

    /**
     * the waits of `addWait()`
     */
    std::vector<cvk::SemaphoreSubmitInfo> _extraWaits{};

    //TODO review behind a std::unique_ptr: the mutex is not movable, dev::FrameSlot::stages moves the stages;
    // the address stays for the graph job and the waiters
    /**
     * set by the graph job, see `submitted()`
     */
    std::unique_ptr<dev::SubmitMark> _submitMarkPtr;

public:
    Stage(Stage const& other) = delete;
    Stage(Stage&& other) noexcept = default;
    Stage& operator=(Stage const& other) = delete;
    Stage& operator=(Stage&& other) noexcept = default;
};
}
