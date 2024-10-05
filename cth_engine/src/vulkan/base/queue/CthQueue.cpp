#include "CthQueue.hpp"

#include "CthPresentInfo.hpp"
#include "CthSubmitInfo.hpp"
#include "vulkan/surface/swapchain/CthBasicSwapchain.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"



namespace cth::vk {

Queue::~Queue() { optDestroy(); }
void Queue::wrap(State const& state) {
    optDestroy();

    _handle = state.vkQueue.get();
    _familyIndex = state.familyIndex;
    _queueIndex = state.queueIndex;
}
void Queue::destroy() {
    debug_check(this);
    reset();
}
Queue::State Queue::release() {
    debug_check(this);
    State const state{
        _handle.release(),
        _familyIndex,
        _queueIndex,
    };
    reset();
    return state;
}

void Queue::submit(SubmitInfo& submit_info) const { const_submit(submit_info.next()); }
void Queue::const_submit(SubmitInfo const& submit_info) const { submit(submit_info.get(), submit_info.fence()); }
void Queue::skip(SubmitInfo& submit_info) const { const_skip(submit_info.next()); }
void Queue::const_skip(SubmitInfo const& submit_info) const { submit(submit_info.skip(), submit_info.fence()); }


VkResult Queue::present(uint32_t image_index, PresentInfo& present_info) const {

    auto const result = vkQueuePresentKHR(get(), present_info.create(image_index));

    CTH_STABLE_ERR(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR, "failed to present")
        throw cth::vk::result_exception{result, details->exception()};

    return result;
}
void Queue::const_skip(PresentInfo const& present_info) const {
    auto const result = vkQueueSubmit(get(), 1, present_info.skip(), VK_NULL_HANDLE);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to skip-present")
        throw cth::vk::result_exception{result, details->exception()};
}
void Queue::reset() {
    _handle = VK_NULL_HANDLE;
    _familyIndex = 0;
    _queueIndex = 0;
}

void Queue::submit(VkSubmitInfo const* submit_info, VkFence fence) const {
    debug_check(this);
    auto const result = vkQueueSubmit(_handle.get(), 1, submit_info, fence);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to submit info to queue")
        throw cth::vk::result_exception{result, details->exception()};
}

} //namespace cth
