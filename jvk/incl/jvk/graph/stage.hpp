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
#include <span>
#include <type_traits>
#include <utility>
#include <vector>


namespace jvk {
class StageGraph;
}

namespace jvk::dev {
/**
 * resources of a stage in one frame slot
 * @details owned by the graph, they live as long as the graph
 * @details @ref Stage uses them for one frame
 */
struct StageSlot {
    cth::not_null_ptr<StageGraph> graphPtr;

    /**
     * id of the stage, see @ref StageGraphConfig::stages
     */
    size_t stageId;

    size_t slot;
    cth::not_null_ptr<Queue const> queuePtr;

    //TODO review a pointer: one semaphore per stage (2026-11-09), owned by the graph, shared by the frame slots
    /**
     * semaphore of the stage, owned by the graph, the submit or the skip of frame N signals N
     */
    cth::not_null_ptr<Semaphore const> semaphorePtr;

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
     * destroyed when @ref semaphorePtr reaches the frame number of the next submit
     */
    Stasis stasis;
};
}

namespace jvk {
/**
 * submission of one stage in one frame
 * @details @ref StageGraph::nextFrame() constructs it per frame, @ref Frame::stage() hands it out
 * @details the resources come from its stage slot
 * @details valid until @ref StageGraph::nextFrame() hands out its slot again
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

    //TODO review names addWait() and extraWaits(): waits() could be read as the waits of the config
    //TODO review the extra waits live in the stage (per frame), not in the stage slot (reused capacity)
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
     * @details returns at once, the strand of the queue submits after the waited stages submitted
     * @details the submit resolves the first uses of the primary against the states of the graph
     * @details waits for the stage waits of the config and the extra waits, see @ref addWait()
     * @details signals @ref semaphore() with @ref frameNumber()
     * @details after the gpu finished the submit: destroys the retained objects
     * @pre @ref beginPrimary() in this frame
     * @pre @ref status() is UNSUBMITTED
     * @pre the primary has no open rendering
     * @note ends the primary
     */
    void submit();

    //TODO review the empty submit waits for the extra waits (an acquire semaphore must not stay signaled),
    // open: the stage waits of the config (a stage that waits for the skip waits for them through it)
    //TODO review the retained objects: destroyed at once (2026-11-03) or after the empty submit
    /**
     * submits no work in this frame
     * @details an empty submit on the queue of the stage waits for the extra waits, signals @ref semaphore()
     *  with @ref frameNumber()
     * @details destroys the retained objects at once
     * @pre no @ref beginPrimary() in this frame
     * @pre @ref status() is UNSUBMITTED
     */
    void skip();

    /**
     * keeps the object until the gpu finished the next submit of the stage
     * @details a @ref skip() destroys it at once
     * @details thread safe
     * @pre @ref status() is UNSUBMITTED
     * @pre its users on other queues finish before this submit: this submit waits for them
     */
    template<class T>
    requires(!std::is_lvalue_reference_v<T>) && requires(T& object) { object.release().decompose(); }
    void retain(T&& object);

    /**
     * waits until the gpu finished the submit of the frame
     * @details awaits @ref semaphore() for @ref frameNumber(), resumes on the context of the caller
     * @pre @ref status() is not UNSUBMITTED
     */
    [[nodiscard]] cth::co::executor_task<void> wait() const;

    [[nodiscard]] constexpr Status status() const { return _status; }
    [[nodiscard]] constexpr size_t id() const { return _slotPtr->stageId; }
    [[nodiscard]] constexpr size_t slot() const { return _slotPtr->slot; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_slotPtr->queuePtr; }

    //TODO review added: the presenter submits through the graph of the stage
    [[nodiscard]] constexpr StageGraph& graph() const { return *_slotPtr->graphPtr; }

    /**
     * @return semaphore of the stage, the submit or the skip of frame N signals N
     */
    [[nodiscard]] constexpr Semaphore const& semaphore() const { return *_slotPtr->semaphorePtr; }

    /**
     * @return number of the frame, the value of @ref semaphore() that this frame signals
     */
    [[nodiscard]] constexpr uint64_t frameNumber() const { return _frameNumber; }

    /**
     * @return signaled after the vkQueueSubmit2() or the skip of this frame
     */
    [[nodiscard]] constexpr cth::os::fence const& submitted() const { return _slotPtr->submitted; }

    /**
     * @return the waits of @ref addWait()
     */
    [[nodiscard]] constexpr std::span<cvk::SemaphoreSubmitInfo const> extraWaits() const { return _extraWaits; }

private:
    cth::not_null_ptr<dev::StageSlot> _slotPtr;
    uint64_t _frameNumber;
    Status _status = Status::UNSUBMITTED;

    std::vector<cvk::SemaphoreSubmitInfo> _extraWaits{};

public:
    Stage(Stage const& other) = delete;
    Stage(Stage&& other) noexcept = default;
    Stage& operator=(Stage const& other) = delete;
    Stage& operator=(Stage&& other) noexcept = default;
};
}
