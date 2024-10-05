#include "CthSubmitInfo.hpp"

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/control/CthTimelineSemaphore.hpp"
#include "vulkan/render/control/CthWaitStage.hpp"


namespace cth::vk {

SubmitInfo::SubmitInfo(std::span<PrimaryCmdBuffer const* const> cmd_buffers, std::span<PipelineWaitStage const> wait_stages,
    std::span<Semaphore* const> signal_semaphores, Fence const* fence) : _fence(fence) {
    _cmdBuffers.resize(cmd_buffers.size());

    std::ranges::transform(cmd_buffers, _cmdBuffers.begin(), [](PrimaryCmdBuffer const* cmd_buffer) {
        CmdBuffer::debug_check(cmd_buffer);
        return cmd_buffer->get();
    });

    initWait(wait_stages);
    initSignal(signal_semaphores);


    create();
}
SubmitInfo& SubmitInfo::next() {
    std::ranges::transform(_waitTimelineSemaphores, _waitValues.begin(), [](TimelineSemaphore const* semaphore) { return semaphore->value(); });
    std::ranges::transform(_signalTimelineSemaphores, _signalValues.begin(), [](TimelineSemaphore* semaphore) { return semaphore->next(); });

    return *this;
}


void SubmitInfo::createTimelineInfo() {
    CTH_CRITICAL(_waitValues.size() != _waitSemaphores.size() || _signalValues.size() != _signalSemaphores.size(),
        "wait values must be the same size as wait semaphores") {
        details->add("signal values ({0}), semaphores ({1})", _signalValues.size(), _signalSemaphores.size());
        details->add("wait values ({0}), wait semaphores ({1})", _waitValues.size(), _waitSemaphores.size());
        details->add("vulkan docs: https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#VK_KHR_timeline_semaphore");
    }

    _timelineInfo = std::make_unique<VkTimelineSemaphoreSubmitInfo>(
        VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, nullptr,
        static_cast<uint32_t>(_waitValues.size()), _waitValues.data(),
        static_cast<uint32_t>(_signalValues.size()), _signalValues.data()
        );
}


void SubmitInfo::createInfo() {
    CTH_CRITICAL(_timelineInfo->sType != VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, "timeline info invalid or not initialized"){}

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
void SubmitInfo::createSkipSubmitInfo() {
    _skipSubmitInfo = _submitInfo;
    _skipSubmitInfo.pCommandBuffers = nullptr;
    _skipSubmitInfo.commandBufferCount = 0;
}

void SubmitInfo::create() {
    createTimelineInfo();
    createInfo();
    createSkipSubmitInfo();
}


void SubmitInfo::initWait(std::span<PipelineWaitStage const> wait_stages) {
    _waitValues.resize(wait_stages.size());
    _waitSemaphores.reserve(wait_stages.size());

    std::vector<PipelineWaitStage> stages{};
    stages.reserve(wait_stages.size());

    for(auto& waitStage : wait_stages) {
        Semaphore::debug_check(waitStage.semaphore);
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
void SubmitInfo::initSignal(std::span<Semaphore* const> signal_semaphores) {
    _signalValues.resize(signal_semaphores.size());
    _signalSemaphores.reserve(signal_semaphores.size());

    std::vector<VkSemaphore> semaphores{};
    semaphores.reserve(signal_semaphores.size());

    for(auto& signalSemaphore : signal_semaphores) {
        Semaphore::debug_check(signalSemaphore);
        auto semaphore = dynamic_cast<TimelineSemaphore*>(signalSemaphore);
        if(!semaphore) semaphores.push_back(signalSemaphore->get());
        else {
            _signalTimelineSemaphores.push_back(semaphore);
            _signalSemaphores.push_back(semaphore->get());
        }
    }

    _signalSemaphores.insert(_signalSemaphores.end(), semaphores.begin(), semaphores.end());
}



VkFence SubmitInfo::fence() const {
    if(_fence == nullptr) return nullptr;
    return _fence->get();
}

}
