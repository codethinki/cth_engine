#include "jvk/base/queue/queue.hpp"

#include "jvk/base/queue/present_info.hpp"
#include "jvk/base/queue/submit_info.hpp"

#include "jvk/base/device.hpp"

#include "jvk/surface/swapchain/swapchain.hpp"
#include "jvk/utility/vk_exceptions.hpp"



namespace jvk {

Queue::~Queue() { optDestroy(); }

void Queue::wrap(State state) {
    State::debug_check(state);
    optDestroy();


    _handle = state.vkQueue.get();
    _device = state.device.get();
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
    State const state{
        _device,
        _handle.release(),
        _familyIndex,
        _queueIndex,
        std::move(_handleMtx),
    };
    reset();
    return state;
}

void Queue::submit(SubmitInfo& submit_info) const { const_submit(submit_info.next()); }

void Queue::const_submit(SubmitInfo const& submit_info) const { raw_submit(*submit_info.get(), submit_info.fence()); }

void Queue::skip(SubmitInfo& submit_info) const { const_skip(submit_info.next()); }

void Queue::const_skip(SubmitInfo const& submit_info) const { raw_submit(*submit_info.skip(), submit_info.fence()); }


VkResult Queue::present(uint32_t image_index, PresentInfo& present_info) const {
    return raw_present(*present_info.create(image_index));
}
VkResult Queue::raw_present(VkPresentInfoKHR const& present_info) const {
    debug_check_present(*this);

    VkResult result;
    {
        std::unique_lock<std::mutex> lock{};
        if(_handleMtx)
            lock = std::unique_lock{*_handleMtx};


        result = _device->functions()->vkQueuePresentKHR(get(), &present_info);
    }
    JVK_RESULT_STABLE_THROW(
        result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR,
        result,
        "failed to present"
    );

    return result;
}

void Queue::const_skip(PresentInfo const& present_info) const {
    debug_check(*this);
    raw_submit(*present_info.skip(), VK_NULL_HANDLE);
}


void Queue::reset() {
    _device = nullptr;
    _handle = VK_NULL_HANDLE;
    _handleMtx = nullptr;
    _familyIndex = 0;
    _queueIndex = 0;
}

void Queue::raw_submit(VkSubmitInfo const& submit_info, VkFence fence) const {
    debug_check(*this);
    VkResult result;
    {
        std::unique_lock<std::mutex> lock;
        if(_handleMtx)
            lock = std::unique_lock{*_handleMtx};


        result = _device->functions()->vkQueueSubmit(_handle.get(), 1, &submit_info, fence);
    }

    JVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to submit info to queue");
}
void Queue::wait() const {
    debug_check(*this);
    _device->functions()->vkQueueWaitIdle(_handle.get());
}

} //namespace cth
