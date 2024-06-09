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

using std::vector;

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
void Queue::debug_check_present_queue(const Queue* queue) {
    CTH_ERR(queue == nullptr, "queue invalid (nullptr)") throw details->exception();
    CTH_ERR(!(queue->familyProperties() & QUEUE_FAMILY_PROPERTY_PRESENT), "queue is not a present queue") throw details->exception();
}

#endif


} //namespace cth

//SubmitInfo

namespace cth {
using std::span;


Queue::SubmitInfo::SubmitInfo(std::span<const PrimaryCmdBuffer* const> cmd_buffers, const std::span<const PipelineWaitStage> wait_stages,
    const std::span<BasicSemaphore* const> signal_semaphores, const BasicFence* fence) : _fence(fence) {
    _cmdBuffers.resize(cmd_buffers.size());

    std::ranges::transform(cmd_buffers, _cmdBuffers.begin(), [](const PrimaryCmdBuffer* cmd_buffer) {
        DEBUG_CHECK_CMD_BUFFER(cmd_buffer);
        return cmd_buffer->get();
    });

    initSignal(signal_semaphores);
    initWait(wait_stages);


    _timelineInfo = createTimelineInfo();
    _submitInfo = createInfo();
}
const VkSubmitInfo* Queue::SubmitInfo::next() {
    std::ranges::transform(_waitTimelineSemaphores, _waitValues.begin(), [](const TimelineSemaphore* semaphore) { return semaphore->value(); });
    std::ranges::transform(_signalTimelineSemaphores, _signalValues.begin(), [](TimelineSemaphore* semaphore) { return semaphore->next(); });

    return &_submitInfo;
}

VkSubmitInfo Queue::SubmitInfo::createInfo() const {
    return VkSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &_timelineInfo,
        .waitSemaphoreCount = static_cast<uint32_t>(_waitSemaphores.size()),
        .pWaitSemaphores = _waitSemaphores.data(),
        .pWaitDstStageMask = _pipelineWaitStages.data(),

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
void Queue::SubmitInfo::initSignal(const std::span<BasicSemaphore* const> signal_semaphores) {
    _signalValues.resize(signal_semaphores.size());
    _signalSemaphores.reserve(signal_semaphores.size());

    vector<VkSemaphore> semaphores{};
    semaphores.reserve(signal_semaphores.size());

    for(auto& signalSemaphore : signal_semaphores) {
        DEBUG_CHECK_SEMAPHORE(signalSemaphore);
        auto semaphore = dynamic_cast<TimelineSemaphore*>(signalSemaphore);
        if(!semaphore) semaphores.push_back(signalSemaphore->get());
        else {
            _signalTimelineSemaphores.push_back(semaphore);
            _signalSemaphores.push_back(semaphore->get());
        }
    }

    _signalSemaphores.insert(_signalSemaphores.end(), semaphores.begin(), semaphores.end());
}
void Queue::SubmitInfo::initWait(const std::span<const PipelineWaitStage> wait_stages) {
    _waitValues.resize(wait_stages.size());
    _waitSemaphores.reserve(wait_stages.size());

    vector<PipelineWaitStage> stages{};
    stages.reserve(wait_stages.size());

    for(auto& waitStage : wait_stages) {
        DEBUG_CHECK_SEMAPHORE(waitStage.semaphore);
        auto semaphore = dynamic_cast<const TimelineSemaphore*>(waitStage.semaphore);
        if(!semaphore) stages.push_back(waitStage);
        else {
            _waitTimelineSemaphores.push_back(semaphore);
            _waitSemaphores.push_back(semaphore->get());
            _pipelineWaitStages.push_back(waitStage.stage);
        }
    }

    std::ranges::transform(stages, std::ranges::begin(std::views::zip(_waitSemaphores, _pipelineWaitStages)), [](const PipelineWaitStage& stage) {
        return std::pair{stage.semaphore->get(), stage.stage};
    });
}



} //namespace cth

//PresentInfo

namespace cth {
Queue::PresentInfo::PresentInfo(const BasicSwapchain& swapchain, std::span<const BasicSemaphore*> wait_semaphores) : _swapchain(swapchain.get()) {
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
