#include "CthQueue.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/surface/CthBasicSwapchain.hpp"

#include <algorithm>
#include <utility>
#include<vector>
#include <cth/cth_log.hpp>

#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/control/CthTimelineSemaphore.hpp"


namespace cth {
class BasicSemaphore;
class TimelineSemaphore;


void Queue::wrap(const uint32_t family_index, const uint32_t queue_index, VkQueue vk_queue) {
    _familyIndex = family_index;
    _queueIndex = queue_index;
    _handle = vk_queue;
}
VkResult Queue::submit(const SubmitInfo& submit_info) const { return vkQueueSubmit(_handle, 1, submit_info.get(), submit_info.fence()); }


VkResult Queue::present(const uint32_t image_index, const PresentInfo& present_info) const {
    VkResult result = VK_SUCCESS;

    auto info = present_info.createInfo(&image_index, &result);

    return vkQueuePresentKHR(get(), &info);
}


#ifdef CONSTANT_DEBUG_MODE
void Queue::debug_check(const Queue* queue) { CTH_ERR(queue == nullptr, "queue invalid (nullptr)"); }
void Queue::debug_check_present_queue(const Queue* queue) {}

#endif


} //namespace cth

//SubmitInfo

namespace cth {
using std::span;


Queue::SubmitInfo::SubmitInfo(std::span<const PrimaryCmdBuffer* const> cmd_buffers, std::span<const WaitStage> wait_stages,
    std::span<BasicSemaphore* const> signal_semaphores, const BasicFence* fence) : _fence(fence) {
    _cmdBuffers.resize(cmd_buffers.size());

    std::ranges::transform(cmd_buffers, _cmdBuffers.begin(), [](const PrimaryCmdBuffer* cmd_buffer) {
        DEBUG_CHECK_CMD_BUFFER(cmd_buffer);
        return cmd_buffer->get();
    });


    _waitStages.resize(wait_stages.size());
    std::ranges::transform(wait_stages, std::ranges::begin(std::views::zip(_waitSemaphores, _waitStages)), [](const WaitStage& stage) {
        DEBUG_CHECK_SEMAPHORE(stage.semaphore);
        return std::pair{stage.semaphore->get(), stage.stage};
    });

    _signalSemaphores.resize(signal_semaphores.size());
    std::ranges::transform(signal_semaphores, _signalSemaphores.begin(), [](const BasicSemaphore* semaphore) {
        DEBUG_CHECK_SEMAPHORE(semaphore);
        return semaphore->get();
    });


    _timelineInfo = createTimelineInfo();
    _submitInfo = createInfo();
}
const VkSubmitInfo* Queue::SubmitInfo::next() {


    std::vector<size_t> waitValues(_waitSemaphores.size());
    std::ranges::transform(_waitTimelineSemaphores, _waitValues.begin(), [](const TimelineSemaphore* semaphore) { return semaphore->value(); });

    std::vector<size_t> signalValues(_signalSemaphores.size());
    std::ranges::transform(_signalTimelineSemaphores, _signalValues.begin(), [](TimelineSemaphore* semaphore) { return semaphore->next(); });


}

VkSubmitInfo Queue::SubmitInfo::createInfo() const {
    return VkSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &_timelineInfo,
        .waitSemaphoreCount = static_cast<uint32_t>(_waitSemaphores.size()),
        .pWaitSemaphores = _waitSemaphores.data(),
        .pWaitDstStageMask = _waitStages.data(),

        .commandBufferCount = static_cast<uint32_t>(_cmdBuffers.size()),
        .pCommandBuffers = _cmdBuffers.data(),

        .signalSemaphoreCount = static_cast<uint32_t>(_signalSemaphores.size()),
        .pSignalSemaphores = _signalSemaphores.data(),
    };
}
VkTimelineSemaphoreSubmitInfo Queue::SubmitInfo::createTimelineInfo() const {
    const VkTimelineSemaphoreSubmitInfo timelineInfo{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .signalSemaphoreValueCount = static_cast<uint32_t>(_signalValues.size()),
        .pSignalSemaphoreValues = _signalValues.data(),
        .waitSemaphoreValueCount = static_cast<uint32_t>(_waitValues.size()),
        .pWaitSemaphoreValues = _waitValues.data(),
    };
    return timelineInfo;
}
void Queue::SubmitInfo::initSignal(std::span<BasicSemaphore* const> signal_semaphores) {
    _signalValues.resize(signal_semaphores.size());

    _signalTimelineSemaphores = signal_semaphores |
        std::views::filter([](BasicSemaphore* semaphore) { return dynamic_cast<TimelineSemaphore*>(semaphore); }) |
        std::ranges::to<std::vector<TimelineSemaphore*>>();


}



} //namespace cth

//PresentInfo

namespace cth {
Queue::PresentInfo::PresentInfo(const BasicSwapchain& swapchain, std::span<const BasicSemaphore*> wait_semaphores) {
    _swapchain = swapchain.get();

    _waitSemaphores.resize(wait_semaphores.size());
    std::ranges::transform(wait_semaphores, _waitSemaphores.begin(), [](const BasicSemaphore* semaphore) { return semaphore->get(); });

}
VkPresentInfoKHR Queue::PresentInfo::createInfo(const uint32_t* image_index, VkResult* result) const {
    return VkPresentInfoKHR{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1u,
        .pSwapchains = &_swapchain,
        .pImageIndices = image_index,
        .pResults = result,
    };
}

}
