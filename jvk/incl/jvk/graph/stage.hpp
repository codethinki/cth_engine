#pragma once
#include "jvk/base/queue.hpp"
#include "jvk/base/stasis.hpp"
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/base/queue/submit_info.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/os/fence.hpp>
#include <cth/ptr/not_null.hpp>

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
struct StageSlot;

/**
 * wait of a stage slot for the slot of the waited stage, from a @ref StageWait
 */
struct SlotWait {
    /**
     * the waited stage in the frame slot of the waited frame
     */
    cth::not_null_ptr<StageSlot const> slotPtr;

    /**
     * see @ref StageWait::stages
     */
    VkPipelineStageFlags2 stages;

    /**
     * see @ref StageWait::frameOffset
     */
    uint32_t frameOffset;
};

/**
 * resources of a stage in one frame slot
 * @details
 * - owned by the graph, live as long as the graph
 * - the @ref Stage of each frame uses them
 */
struct StageSlot {
    cth::not_null_ptr<StageGraph> graphPtr;

    /**
     * id of the stage, see @ref StageGraphConfig::stages
     */
    size_t stageId;

    size_t slot;
    cth::not_null_ptr<Queue const> queuePtr;

    /**
     * semaphore of the stage, owned by the graph
     * @details the submit or the skip of frame N signals N
     */
    cth::not_null_ptr<Semaphore const> semaphorePtr;

    /**
     * one per @ref StageConfig::waits
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
     * @ref StageConfig::secondaries of them
     */
    std::vector<SecondaryCmdBuffer> secondaries{};

    /**
     * signaled after the vkQueueSubmit2() or the skip of the frame
     */
    cth::os::fence submitted{};

    std::mutex stasisMutex{};

    /**
     * destroyed when @ref semaphorePtr reaches the frame number of the next submit or skip
     */
    Stasis stasis;
};
}

namespace jvk {
/**
 * submission of one stage in one frame
 * @details
 * - per frame: constructed by @ref StageGraph::nextFrame(), handed out by @ref Frame::stage()
 * - the resources come from its stage slot
 * - valid until @ref StageGraph::nextFrame() hands out its slot again
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
     * @pre slotPtr != nullptr
     */
    Stage(dev::StageSlot* slotPtr, uint64_t frameNumber);

    /**
     * begins the primary command buffer of the frame
     * @details resets the primary at each begin
     * @return the primary
     * @pre @ref status() is UNSUBMITTED
     * @attention not thread safe
     */
    PrimaryCmdBuffer& beginPrimary();

    /**
     * primary command buffer of the frame slot
     * @details not begun, see @ref beginPrimary()
     */
    [[nodiscard]] PrimaryCmdBuffer& primary() { return _slotPtr->primary; }

    //TODO review std::optional instead of a default inheritance {}: an empty inheritance continues a rendering
    /**
     * begins the secondary command buffer of the frame slot
     * @param inheritanceOpt std::nullopt -> for use outside of a rendering
     * @return the secondary
     * @pre the preconditions of @ref secondary()
     * @attention not thread safe for one secondaryIdx
     */
    SecondaryCmdBuffer& beginSecondary(
        size_t secondaryIdx,
        std::optional<Inheritance> const& inheritanceOpt = std::nullopt
    );

    /**
     * secondary command buffer of the frame slot
     * @details not begun, see @ref beginSecondary()
     * @details the same index in the next frames of the slot is the same secondary
     * @pre secondaryIdx < @ref StageConfig::secondaries
     * @pre @ref primary() executes it before the @ref submit()
     */
    [[nodiscard]] SecondaryCmdBuffer& secondary(size_t secondaryIdx);

    /**
     * secondary command buffer for the queue family of the stage, owned by the caller
     * @details not begun, see @ref SecondaryCmdBuffer::begin()
     * @throws cvk::vk_result_exception result of vkCreateCommandPool() or vkAllocateCommandBuffers()
     */
    [[nodiscard]] SecondaryCmdBuffer persistentSecondary() const;

    /**
     * adds a semaphore wait to the submit of this frame
     * @details @ref submit() and @ref skip() wait for it
     * @pre @ref status() is UNSUBMITTED
     * @pre the semaphore lives until the gpu finished the submit
     * @attention not thread safe
     */
    void addWait(cvk::SemaphoreSubmitInfo const&);

    /**
     * submits the primary of the frame
     * @details calls @ref StageGraph::submit() of the graph of the stage
     * @pre the preconditions of @ref StageGraph::submit()
     */
    void submit();

    /**
     * submits no work in this frame
     * @details calls @ref StageGraph::skip() of the graph of the stage
     * @pre the preconditions of @ref StageGraph::skip()
     */
    void skip();

    /**
     * keeps the object until the gpu finished the submit or the skip of this frame
     * @details thread safe
     * @pre @ref status() is UNSUBMITTED
     * @pre its users on other queues finish before this submit: this submit waits for them
     */
    template<class T>
    requires(!std::is_lvalue_reference_v<T>) && requires(T& object) { object.release().decompose(); }
    void retain(T&& object);

    /**
     * waits until the gpu finished the submit or the skip of this frame
     * @details resumes on the context of the caller
     * @pre @ref status() is not UNSUBMITTED
     */
    [[nodiscard]] cth::co::executor_task<void> wait() const;

    [[nodiscard]] constexpr Status status() const { return _status; }
    [[nodiscard]] constexpr size_t id() const { return _slotPtr->stageId; }
    [[nodiscard]] constexpr size_t slot() const { return _slotPtr->slot; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_slotPtr->queuePtr; }

    /**
     * @return semaphore of the stage
     * @details the submit or the skip of frame N signals N
     */
    [[nodiscard]] constexpr Semaphore const& semaphore() const { return *_slotPtr->semaphorePtr; }

    /**
     * @return number of the frame, the value of @ref semaphore() for this frame
     */
    [[nodiscard]] constexpr uint64_t frameNumber() const { return _frameNumber; }

    /**
     * @return signaled after the vkQueueSubmit2() or the skip of this frame
     */
    [[nodiscard]] constexpr cth::os::fence const& submitted() const { return _slotPtr->submitted; }

private:
    // the graph sets the status of the submission, there is no other access without a public setter
    friend class StageGraph;

    cth::not_null_ptr<dev::StageSlot> _slotPtr;
    uint64_t _frameNumber;
    Status _status = Status::UNSUBMITTED;

    /**
     * the waits of @ref addWait()
     */
    std::vector<cvk::SemaphoreSubmitInfo> _extraWaits{};

public:
    Stage(Stage const& other) = delete;
    Stage(Stage&& other) noexcept = default;
    Stage& operator=(Stage const& other) = delete;
    Stage& operator=(Stage&& other) noexcept = default;
};
}
