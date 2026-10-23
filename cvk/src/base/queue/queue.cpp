#include "cvk/base/queue/queue.hpp"

#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <concepts>
#include <variant>
#include <vector>


namespace cvk {
using std::span;

namespace {
    VkSemaphoreSubmitInfo vk_semaphore_info(VkSemaphore vkSemaphore, uint64_t value, VkPipelineStageFlags2 stages) {
        return VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = vkSemaphore,
            .value = value,
            .stageMask = stages,
            .deviceIndex = 0,
        };
    }

    /**
     * @param resolve the value of a timeline semaphore without value
     */
    template<class Resolve>
    VkSemaphoreSubmitInfo semaphore_info(SemaphoreSubmitInfo const& info, Resolve resolve) {
        return std::visit(
            [&]<class T>(not_null_ptr<T const> semaphore) {
                T::debug_check(*semaphore);

                if constexpr(std::same_as<T, TimelineSemaphore>) {
                    auto const value = info.value.has_value() ? *info.value : resolve(*semaphore);
                    return vk_semaphore_info(semaphore->get(), value, info.stages);
                } else {
                    CTH_CRITICAL(info.value.has_value(), "binary semaphores have no value") {}
                    return vk_semaphore_info(semaphore->get(), 0, info.stages);
                }
            },
            info.semaphore
        );
    }
}

Queue::Queue(QueueFamilyProperties familyProperties, State const& state) : _familyProperties{familyProperties},
    _functions{state.functions.get()},
    _handle{state.vkQueue.get()},
    _familyIndex{state.familyIndex},
    _queueIndex{state.queueIndex} {}

void Queue::submit(span<SubmitInfo const> infos, Fence const* fence) const {
    debug_check(*this);
    if(fence != nullptr) Fence::debug_check(*fence);

    size_t cmdBufferCount = 0;
    size_t semaphoreCount = 0;
    for(auto const& info : infos) {
        cmdBufferCount += info.cmdBuffers.size();
        semaphoreCount += info.waits.size() + info.signals.size();
    }

    // reserved: the submit infos point into the vectors
    std::vector<VkCommandBufferSubmitInfo> cmdBuffers{};
    cmdBuffers.reserve(cmdBufferCount);
    std::vector<VkSemaphoreSubmitInfo> semaphores{};
    semaphores.reserve(semaphoreCount);
    std::vector<VkSubmitInfo2> submits{};
    submits.reserve(infos.size());

    auto const waitValue = [](TimelineSemaphore const& timeline) { return timeline.value(); };
    auto const signalValue = [](TimelineSemaphore const& timeline) { return timeline.next(); };

    for(auto const& info : infos) {
        auto const* const waits = semaphores.data() + semaphores.size();
        for(auto const& wait : info.waits)
            semaphores.push_back(semaphore_info(wait, waitValue));

        auto const* const signals = semaphores.data() + semaphores.size();
        for(auto const& signal : info.signals)
            semaphores.push_back(semaphore_info(signal, signalValue));

        auto const* const buffers = cmdBuffers.data() + cmdBuffers.size();
        for(auto const& cmdBuffer : info.cmdBuffers) {
            BaseCmdBuffer::debug_check(*cmdBuffer);
            CTH_CRITICAL(cmdBuffer->recording(), "command buffers must end their recording first") {}

            cmdBuffers.push_back(
                VkCommandBufferSubmitInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .pNext = nullptr,
                    .commandBuffer = cmdBuffer->get(),
                    .deviceMask = 0,
                }
            );
        }

        submits.push_back(
            VkSubmitInfo2{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext = nullptr,
                .flags = 0,
                .waitSemaphoreInfoCount = static_cast<uint32_t>(info.waits.size()),
                .pWaitSemaphoreInfos = waits,
                .commandBufferInfoCount = static_cast<uint32_t>(info.cmdBuffers.size()),
                .pCommandBufferInfos = buffers,
                .signalSemaphoreInfoCount = static_cast<uint32_t>(info.signals.size()),
                .pSignalSemaphoreInfos = signals,
            }
        );
    }

    auto const result = _functions->vkQueueSubmit2(
        _handle.get(),
        static_cast<uint32_t>(submits.size()),
        submits.data(),
        fence != nullptr ? fence->get() : VK_NULL_HANDLE
    );

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to submit to queue") {}
}

void Queue::rawSubmit(VkSubmitInfo2 const& submitInfo, VkFence fence) const {
    debug_check(*this);

    auto const result = _functions->vkQueueSubmit2(_handle.get(), 1, &submitInfo, fence);

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to submit to queue") {}
}

VkResult Queue::rawPresent(VkPresentInfoKHR const& presentInfo) const {
    debug_check_present(*this);

    auto const result = _functions->vkQueuePresentKHR(_handle.get(), &presentInfo);

    CVK_RESULT_STABLE_THROW(
        result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR,
        result,
        "failed to present"
    ) {}

    return result;
}

void Queue::waitIdle() const {
    debug_check(*this);

    auto const result = _functions->vkQueueWaitIdle(_handle.get());

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to wait for queue") {}
}
}
