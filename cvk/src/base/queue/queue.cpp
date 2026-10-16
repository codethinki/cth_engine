#include "cvk/base/queue/queue.hpp"

#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
void Queue::wrap(State state) {
    State::debug_check(state);
    optDestroy();

    _handle = state.vkQueue.get();
    _functions = state.functions.get();
    _handleMtx = std::move(state.vkQueueMtx);
    _familyIndex = state.familyIndex;
    _queueIndex = state.queueIndex;
}

void Queue::destroy() {
    debug_check(*this);
    reset();
}

Queue::State Queue::release() {
    debug_check(*this);
    State state{
        _functions,
        _handle.get(),
        _familyIndex,
        _queueIndex,
        std::move(_handleMtx),
    };
    reset();
    return state;
}

void Queue::raw_submit(VkSubmitInfo2 const& submitInfo, VkFence fence) const {
    debug_check(*this);

    VkResult result;
    {
        std::scoped_lock const lock{*_handleMtx};
        result = _functions->vkQueueSubmit2(_handle.get(), 1, &submitInfo, fence);
    }

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to submit to queue") {}
}

VkResult Queue::raw_present(VkPresentInfoKHR const& presentInfo) const {
    debug_check_present(*this);

    VkResult result;
    {
        std::scoped_lock const lock{*_handleMtx};
        result = _functions->vkQueuePresentKHR(_handle.get(), &presentInfo);
    }

    CVK_RESULT_STABLE_THROW(
        result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR,
        result,
        "failed to present"
    ) {}

    return result;
}

void Queue::wait() const {
    debug_check(*this);

    VkResult result;
    {
        std::scoped_lock const lock{*_handleMtx};
        result = _functions->vkQueueWaitIdle(_handle.get());
    }

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to wait for queue") {}
}

void Queue::reset() {
    _functions = nullptr;
    _handle = VK_NULL_HANDLE;
    _handleMtx = nullptr;
    _familyIndex = 0;
    _queueIndex = 0;
}
}
