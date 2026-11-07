#pragma once
#include "jvk/base/stasis.hpp"
#include "jvk/cmd/cmd_buffer.hpp"

#include "cvk/render/sync/fence.hpp"

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
class Queue;
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
     * fence of the submit, exports its OS handle
     */
    cvk::Fence fence;

    /**
     * signaled after the vkQueueSubmit2() or the skip of the frame
     */
    cth::os::fence submitted{};

    std::mutex stasisMutex{};

    /**
     * destroyed after the fence of the next submit
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

    //TODO review begun with the inheritance, while the slot secondaries are not begun
    /**
     * secondary command buffer for the queue family of the stage, owned by the caller, begun with the inheritance
     * @param usage VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT -> several pending primaries execute it
     * @throws cvk::vk_result_exception result of vkCreateCommandPool() or vkAllocateCommandBuffers()
     */
    [[nodiscard]] SecondaryCmdBuffer persistentSecondary(Inheritance const&, VkCommandBufferUsageFlags usage = 0) const;

    /**
     * submits the primary of the frame
     * @details returns at once, the strand of the queue submits after the waited stages submitted
     * @details the submit resolves the first uses of the primary against the states of the graph
     * @details acquired the swapchain image of the frame -> records its transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
     * @details after the fence: destroys the retained objects
     * @pre @ref beginPrimary() in this frame
     * @pre @ref status() is UNSUBMITTED
     * @pre the primary has no open rendering
     * @note ends the primary
     */
    void submit();

    /**
     * submits nothing in this frame
     * @details the stages that wait for this stage do not wait for its gpu work
     * @details destroys the retained objects at once
     * @pre no @ref beginPrimary() in this frame
     * @pre @ref status() is UNSUBMITTED
     * @pre the stage did not acquire the swapchain image of the frame
     */
    void skip();

    /**
     * keeps the object until the fence of the next submit of the stage
     * @details a @ref skip() destroys it at once
     * @details thread safe
     * @pre @ref status() is UNSUBMITTED
     * @pre its users on other queues finish before this submit: this submit waits for them
     */
    template<class T>
    requires(!std::is_lvalue_reference_v<T>) && requires(T& object) { object.release().decompose(); }
    void retain(T&& object);

    /**
     * waits until the gpu finished the submit of the frame, resumes on a worker
     * @pre @ref status() is not UNSUBMITTED
     */
    [[nodiscard]] cth::co::executor_task<void> wait() const;

    [[nodiscard]] constexpr Status status() const { return _status; }
    [[nodiscard]] constexpr size_t id() const { return _slotPtr->stageId; }
    [[nodiscard]] constexpr size_t slot() const { return _slotPtr->slot; }

private:
    cth::not_null_ptr<dev::StageSlot> _slotPtr;
    uint64_t _frameNumber;
    Status _status = Status::UNSUBMITTED;

public:
    Stage(Stage const& other) = delete;
    Stage(Stage&& other) noexcept = default;
    Stage& operator=(Stage const& other) = delete;
    Stage& operator=(Stage&& other) noexcept = default;
};
}
