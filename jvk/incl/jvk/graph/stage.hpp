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

/**
 * stage of one frame slot
 * @details one submit per frame on the queue of the stage
 * @details the graph creates one per stage and frame slot, @ref Frame::stage() hands it out
 * @details valid until @ref StageGraph::nextFrame() hands out its slot again
 */
class Stage {
public:
    using Inheritance = SecondaryCmdBuffer::Inheritance;

    /**
     * creates the command buffers and the fence of the stage slot
     * @details the graph constructs one per stage and frame slot
     * @param imageStage the stage writes the swapchain image
     * @throws cvk::vk_result_exception result of a create of the command buffers or the fence
     */
    Stage(StageGraph&, size_t stageId, size_t slot, Queue const&, bool imageStage);

    /**
     * begins the primary command buffer of the frame
     * @details resets the primary at each begin
     * @return the primary
     * @pre no @ref submit() or @ref skip() in this frame
     * @attention not thread safe
     */
    PrimaryCmdBuffer& beginPrimary();

    /**
     * primary command buffer of the frame slot
     * @details not begun, see @ref beginPrimary()
     */
    [[nodiscard]] PrimaryCmdBuffer& primary() { return _primary; }

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
     * @details image stage: records the transition of the swapchain image to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
     * @details after the fence: destroys the retained objects
     * @pre @ref beginPrimary() in this frame
     * @pre no @ref submit() or @ref skip() in this frame
     * @pre the primary has no open rendering
     * @note ends the primary
     */
    void submit();

    /**
     * submits nothing in this frame
     * @details the stages that wait for this stage do not wait for its gpu work
     * @details destroys the retained objects at once
     * @pre no @ref beginPrimary(), @ref submit() or @ref skip() in this frame
     */
    void skip();

    /**
     * keeps the object until the fence of the next submit of the stage
     * @details a @ref skip() destroys it at once
     * @pre no @ref submit() or @ref skip() in this frame
     * @pre its users on other queues finish before this submit: this submit waits for them
     */
    template<class T>
    requires(!std::is_lvalue_reference_v<T>) && requires(T& object) { object.release().decompose(); }
    void retain(T&& object);

    /**
     * waits until the gpu finished the submit of the frame, resumes on a worker
     * @pre @ref submit() or @ref skip() in this frame
     */
    [[nodiscard]] cth::co::executor_task<void> wait() const;

    /**
     * prepares the slot for a new frame
     * @details called by @ref StageGraph::nextFrame()
     * @pre the fence of the last submit signaled
     */
    void beginFrame(uint64_t frameNumber);

    //TODO review public for the graph instead of a friend
    /**
     * @return signaled after the vkQueueSubmit2() or the skip of the current frame
     */
    [[nodiscard]] constexpr cth::os::fence const& submitted() const { return _submitted; }

    //TODO review public for the graph instead of a friend
    /**
     * @return no @ref submit() or @ref skip() in the current frame
     */
    [[nodiscard]] constexpr bool pending() const { return _status == Status::OPEN || _status == Status::RECORDING; }

    [[nodiscard]] constexpr size_t id() const { return _id; }
    [[nodiscard]] constexpr size_t slot() const { return _slot; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_queuePtr; }

private:

    /**
     * what the stage did in the current frame
     */
    enum class Status {
        OPEN,
        RECORDING,
        SUBMITTED,
        SKIPPED,
    };

    cth::not_null_ptr<StageGraph> _graphPtr;
    size_t _id;
    size_t _slot;
    cth::not_null_ptr<Queue const> _queuePtr;

    Status _status = Status::OPEN;

    /**
     * barriers for the first uses of the primary, submitted before it
     */
    PrimaryCmdBuffer _prologue;
    PrimaryCmdBuffer _primary;

    /**
     * the stage writes the swapchain image, @ref submit() records its transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
     */
    bool _imageStage;

    /**
     * @ref StageConfig::secondaries of them, created once
     */
    std::vector<SecondaryCmdBuffer> _secondaries{};

    std::mutex _stasisMutex;
    Stasis _stasis;

    /**
     * fence of the submit, exports its OS handle
     */
    cvk::Fence _fence;

    /**
     * signaled after the vkQueueSubmit2() or the skip of the frame
     */
    cth::os::fence _submitted{};

    /**
     * number of the current frame of the slot
     */
    uint64_t _frame = 0;

public:
    Stage(Stage const& other) = delete;
    Stage(Stage&& other) = delete;
    Stage& operator=(Stage const& other) = delete;
    Stage& operator=(Stage&& other) = delete;
};
}
