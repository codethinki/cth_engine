#include "CthQueue.hpp"

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/render/control/CthTimelineSemaphore.hpp"
#include "vulkan/surface/swapchain/CthBasicSwapchain.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {
class BasicSemaphore;
class TimelineSemaphore;

using std::vector;

void Queue::wrap(uint32_t const family_index, uint32_t const queue_index, VkQueue vk_queue) {
    _familyIndex = family_index;
    _queueIndex = queue_index;
    _handle = vk_queue;
}
void Queue::submit(SubmitInfo& submit_info) const { const_submit(submit_info.next()); }
void Queue::const_submit(SubmitInfo const& submit_info) const {
    auto const result = vkQueueSubmit(_handle, 1, submit_info.get(), submit_info.fence());
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to submit info to queue")
        throw cth::except::vk_result_exception{result, details->exception()};
}
void Queue::skip(SubmitInfo& submit_info) const { const_skip(submit_info.next()); }
void Queue::const_skip(SubmitInfo const& submit_info) const {
    auto submitInfo = *submit_info.get();
    submitInfo.commandBufferCount = 0;
    auto const result = vkQueueSubmit(_handle, 1, &submitInfo, submit_info.fence());

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to skip submit to queue")
        throw except::vk_result_exception{result, details->exception()};
}


VkResult Queue::present(uint32_t const image_index, PresentInfo const& present_info) const {
    auto const& info = present_info.createInfo(image_index);

    auto const result = vkQueuePresentKHR(get(), &info);

    CTH_STABLE_ERR(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR, "failed to present")
        throw cth::except::vk_result_exception{result, details->exception()};

    return result;
}


#ifdef CONSTANT_DEBUG_MODE
void Queue::debug_check(Queue const* queue) {
    CTH_ERR(queue == nullptr, "queue invalid (nullptr)") throw details->exception();
}
void Queue::debug_check_present_queue(Queue const* queue) {
    CTH_ERR(queue == nullptr, "queue invalid (nullptr)") throw details->exception();
    CTH_ERR(!(queue->familyProperties() & QUEUE_FAMILY_PROPERTY_PRESENT), "queue is not a present queue") throw details->exception();
}

#endif


} //namespace cth

//SubmitInfo

namespace cth::vk {
using std::span;


Queue::SubmitInfo::SubmitInfo(std::span<PrimaryCmdBuffer const* const> cmd_buffers, std::span<PipelineWaitStage const> const wait_stages,
    std::span<BasicSemaphore* const> const signal_semaphores, BasicFence const* fence) : _fence(fence) {
    _cmdBuffers.resize(cmd_buffers.size());

    std::ranges::transform(cmd_buffers, _cmdBuffers.begin(), [](PrimaryCmdBuffer const* cmd_buffer) {
        DEBUG_CHECK_CMD_BUFFER(cmd_buffer);
        return cmd_buffer->get();
    });

    initWait(wait_stages);
    initSignal(signal_semaphores);


    create();
}
Queue::SubmitInfo& Queue::SubmitInfo::next() {
    std::ranges::transform(_waitTimelineSemaphores, _waitValues.begin(), [](TimelineSemaphore const* semaphore) { return semaphore->value(); });
    std::ranges::transform(_signalTimelineSemaphores, _signalValues.begin(), [](TimelineSemaphore* semaphore) { return semaphore->next(); });

    return *this;
}


void Queue::SubmitInfo::createTimelineInfo() {
    CTH_ERR(_waitValues.size() != _waitSemaphores.size() || _signalValues.size() != _signalSemaphores.size(),
        "wait values must be the same size as wait semaphores") {
        details->add("signal values ({0}), semaphores ({1})", _signalValues.size(), _signalSemaphores.size());
        details->add("wait values ({0}), wait semaphores ({1})", _waitValues.size(), _waitSemaphores.size());
        details->add("vulkan docs: https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#VK_KHR_timeline_semaphore");
        throw details->exception();
    }

    _timelineInfo = std::make_unique<VkTimelineSemaphoreSubmitInfo>(
        VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, nullptr,
        static_cast<uint32_t>(_waitValues.size()), _waitValues.data(),
        static_cast<uint32_t>(_signalValues.size()), _signalValues.data()
        );
}


void Queue::SubmitInfo::createInfo() {
    CTH_ERR(_timelineInfo->sType != VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, "timeline info invalid or not initialized")
        throw details->exception();

    _submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = _timelineInfo.get(),

        .waitSemaphoreCount = static_cast<uint32_t>(_waitSemaphores.size()),
        .pWaitSemaphores = _waitSemaphores.data(),
        .pWaitDstStageMask = _pipelineWaitStages.data(),

        .commandBufferCount = static_cast<uint32_t>(_cmdBuffers.size()),
        .pCommandBuffers = _cmdBuffers.data(),

        .signalSemaphoreCount = static_cast<uint32_t>(_signalSemaphores.size()),
        .pSignalSemaphores = _signalSemaphores.data(),
    };
}

void Queue::SubmitInfo::create() {
    createTimelineInfo();
    createInfo();
}


void Queue::SubmitInfo::initWait(std::span<PipelineWaitStage const> const wait_stages) {
    _waitValues.resize(wait_stages.size());
    _waitSemaphores.reserve(wait_stages.size());

    vector<PipelineWaitStage> stages{};
    stages.reserve(wait_stages.size());

    for(auto& waitStage : wait_stages) {
        DEBUG_CHECK_SEMAPHORE(waitStage.semaphore);
        auto semaphore = dynamic_cast<TimelineSemaphore const*>(waitStage.semaphore);
        if(semaphore == nullptr) stages.push_back(waitStage);
        else {
            _waitTimelineSemaphores.push_back(semaphore);
            _waitSemaphores.push_back(semaphore->get());
            _pipelineWaitStages.push_back(waitStage.stage);
        }
    }


    for(auto [stage, semaphore] : stages) {
        _waitSemaphores.push_back(semaphore->get());
        _pipelineWaitStages.push_back(stage);
    }


}
void Queue::SubmitInfo::initSignal(std::span<BasicSemaphore* const> const signal_semaphores) {
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



} //namespace cth

//PresentInfo

namespace cth::vk {
Queue::PresentInfo::PresentInfo(BasicSwapchain const* swapchain, std::span<BasicSemaphore const*> wait_semaphores) : _swapchain(swapchain->get()) {
    _waitSemaphores.resize(wait_semaphores.size());
    std::ranges::transform(wait_semaphores, _waitSemaphores.begin(), [](BasicSemaphore const* semaphore) { return semaphore->get(); });

}
VkPresentInfoKHR Queue::PresentInfo::createInfo(uint32_t const& image_index) const {
    return VkPresentInfoKHR{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1u,
        .pSwapchains = &_swapchain,
        .pImageIndices = &image_index,
        .pResults = nullptr,
    };
}

}
