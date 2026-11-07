#pragma once
#include "cvk/render/sync/fence.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>


namespace jvk {
class Core;

/**
 * fence with an awaitable wait
 * @details owns a @ref cvk::Fence that exports @ref cvk::constants::OS_FENCE_HANDLE_TYPE
 * @details @ref wait() blocks no thread
 */
class Fence {
public:
    /**
     * creates the exportable fence, not signaled
     * @pre @ref Core::created()
     * @throws cvk::vk_result_exception result of vkCreateFence()
     */
    explicit Fence(Core const&);

    //TODO review cth::os::fence has no constructor from an exported handle: the osfence awaiter needs one,
    // until then wait() awaits the handle directly like this_coro::await_handle()
    //TODO review posix: the export resets the fence, a second wait() for the same submit has no pending signal
    /**
     * waits until the fence is signaled, resumes on the context of the caller
     * @details exports the OS handle, an invalid handle -> already signaled
     * @pre a submit that signals the fence is pending or done
     * @throws cvk::vk_result_exception result of vkGetFenceWin32HandleKHR() or vkGetFenceFdKHR()
     */
    [[nodiscard]] cth::co::executor_task<void> wait() const;

    /**
     * sets the fence to not signaled
     * @pre no pending submit signals the fence
     * @throws cvk::vk_result_exception result of vkResetFences()
     */
    void reset() const;

    /**
     * @return the cvk fence, for the submit
     */
    [[nodiscard]] constexpr cvk::Fence const& vk() const { return _fence; }

    [[nodiscard]] constexpr VkFence get() const { return _fence.get(); }

private:
    cvk::Fence _fence;

public:
    Fence(Fence const& other) = delete;
    Fence(Fence&& other) noexcept = default;
    Fence& operator=(Fence const& other) = delete;
    Fence& operator=(Fence&& other) noexcept = default;
};
}
