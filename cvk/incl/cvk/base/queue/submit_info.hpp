#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <vector>


namespace cvk {
class PrimaryCmdBuffer;
class Semaphore;
class TimelineSemaphore;

/**
 * semaphore of a submit
 * @details binary or timeline, the constructor decides
 */
struct SemaphoreSubmitInfo {
    /**
     * @param stages wait -> the stages that wait, signal -> the stages that finish before the signal
     */
    constexpr SemaphoreSubmitInfo(
        Semaphore const& semaphore,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    ) : semaphore{std::cref(semaphore)}, stages{stages} {}

    /**
     * @param stages wait -> the stages that wait, signal -> the stages that finish before the signal
     * @param valueOpt `std::nullopt` -> resolved at the submit,
     *  wait: `TimelineSemaphore::value()`, signal: `TimelineSemaphore::next()`
     */
    constexpr SemaphoreSubmitInfo(
        TimelineSemaphore const& semaphore,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        std::optional<uint64_t> valueOpt = std::nullopt
    ) : semaphore{std::cref(semaphore)}, stages{stages}, value{valueOpt} {}

    std::variant<std::reference_wrapper<Semaphore const>, std::reference_wrapper<TimelineSemaphore const>> semaphore;
    VkPipelineStageFlags2 stages;

    /**
     * timeline semaphores only
     * @attention an explicit value does not change the counter of the semaphore
     */
    std::optional<uint64_t> value = std::nullopt;
};

/**
 * one batch of a queue submit
 */
struct SubmitInfo {
    std::vector<std::reference_wrapper<PrimaryCmdBuffer const>> cmdBuffers{}; ///< the gpu executes them in order

    std::vector<SemaphoreSubmitInfo> waits{};
    std::vector<SemaphoreSubmitInfo> signals{};
};
}
