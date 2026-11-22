#include "cvk/base/queue/submit_info.hpp"

#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"

#include <cth/io/log.hpp>

#include <concepts>
#include <variant>


namespace cvk {
namespace {
    [[nodiscard]] VkSemaphoreSubmitInfo vk_semaphore_info(
        VkSemaphore vkSemaphore,
        uint64_t value,
        VkPipelineStageFlags2 stages
    ) {
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
    [[nodiscard]] VkSemaphoreSubmitInfo semaphore_info(
        SemaphoreSubmitInfo const& info,
        Resolve resolve
    ) {
        return std::visit(
            [&]<class T>(cth::ref_wrapper<T const> semaphore) {
                T::debug_check(semaphore);

                if constexpr(std::same_as<T, TimelineSemaphore>) {
                    auto const value = info.value.has_value()
                        ? *info.value
                        : resolve(semaphore.get());
                    return vk_semaphore_info(semaphore.get().get(), value, info.stages);
                } else {
                    CTH_CRITICAL(info.value.has_value(), "binary semaphores have no value") {}
                    return vk_semaphore_info(semaphore.get().get(), 0, info.stages);
                }
            },
            info.semaphore
        );
    }
}

VkSemaphoreSubmitInfo wait_submit_info(SemaphoreSubmitInfo const& wait) {
    return semaphore_info(
        wait,
        [](TimelineSemaphore const& timeline) { return timeline.value(); }
    );
}

VkSemaphoreSubmitInfo signal_submit_info(SemaphoreSubmitInfo const& signal) {
    return semaphore_info(
        signal,
        [](TimelineSemaphore const& timeline) { return timeline.next(); }
    );
}

VkCommandBufferSubmitInfo cmd_buffer_submit_info(PrimaryCmdBuffer const& cmdBuffer) {
    BaseCmdBuffer::debug_check(cmdBuffer);
    CTH_CRITICAL(cmdBuffer.recording(), "command buffers must end their recording first") {}

    return VkCommandBufferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmdBuffer.get(),
        .deviceMask = 0,
    };
}
}
