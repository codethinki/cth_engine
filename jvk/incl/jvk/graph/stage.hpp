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
#include <deque>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>


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
     * primary command buffer of the frame
     * @details the first call of the frame begins it
     * @pre no @ref submit() or @ref skip() in this frame
     * @attention not thread safe
     */
    [[nodiscard]] PrimaryCmdBuffer& primary();

    /**
     * secondary command buffer of the frame slot, begun with the inheritance
     * @details each call hands out another secondary, the slot reuses them in its next frames
     * @details thread safe
     * @pre no @ref submit() or @ref skip() in this frame
     * @pre @ref primary() executes it before the @ref submit()
     */
    [[nodiscard]] SecondaryCmdBuffer& secondary(Inheritance const&);

    //TODO review default usage, no overload without inheritance
    /**
     * secondary command buffer for the queue family of the stage, owned by the caller, begun with the inheritance
     * @param usage VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT -> several pending primaries execute it
     * @throws cvk::vk_result_exception result of vkCreateCommandPool() or vkAllocateCommandBuffers()
     */
    [[nodiscard]] SecondaryCmdBuffer persistentSecondary(
        Inheritance const&,
        VkCommandBufferUsageFlags usage = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
    ) const;

    /**
     * submits the primary of the frame
     * @details returns at once, the strand of the queue submits after the waited stages submitted
     * @details the submit resolves the first uses of the primary against the states of the graph
     * @details after the fence: destroys the retained objects
     * @pre @ref primary() in this frame
     * @pre no @ref submit() or @ref skip() in this frame
     * @note ends the primary
     */
    void submit();

    /**
     * submits nothing in this frame
     * @details the stages that wait for this stage do not wait for its gpu work
     * @details destroys the retained objects at once
     * @pre no @ref primary(), @ref submit() or @ref skip() in this frame
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

    [[nodiscard]] constexpr size_t id() const { return _id; }
    [[nodiscard]] constexpr size_t slot() const { return _slot; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_queue; }

private:
    friend class StageGraph;

    /**
     * what the stage did in the current frame
     */
    enum class Status {
        OPEN,
        RECORDING,
        SUBMITTED,
        SKIPPED,
    };

    //TODO review construction only by the graph
    Stage(StageGraph&, size_t id, size_t slot, Queue const&, bool imageStage);

    cth::not_null_ptr<StageGraph> _graph;
    size_t _id;
    size_t _slot;
    cth::not_null_ptr<Queue const> _queue;

    Status _status = Status::OPEN;

    /**
     * barriers for the first uses of the primary, submitted before it
     */
    PrimaryCmdBuffer _prologue;
    PrimaryCmdBuffer _primary;

    /**
     * transition of the swapchain image to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, image stage only
     */
    std::optional<PrimaryCmdBuffer> _epilogue;

    std::mutex _secondariesMutex;
    std::deque<SecondaryCmdBuffer> _secondaries{};

    /**
     * secondaries handed out in the current frame
     */
    size_t _usedSecondaries = 0;

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
