#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>


namespace cvk {
class PrimaryCmdBuffer;
class Semaphore;
class TimelineSemaphore;

/**
 * @brief semaphore of a submit, binary or timeline
 * @details the constructor decides the kind
 */
struct SemaphoreSubmitInfo {
    /**
     * @param stages wait -> the stages that wait, signal -> the stages that finish before the signal
     */
    constexpr SemaphoreSubmitInfo(
        Semaphore const& semaphore,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    ) : semaphore{&semaphore}, stages{stages} {}

    /**
     * @param stages wait -> the stages that wait, signal -> the stages that finish before the signal
     * @param value std::nullopt -> resolved at the submit,
     *  wait: @ref TimelineSemaphore::value(), signal: @ref TimelineSemaphore::next()
     */
    constexpr SemaphoreSubmitInfo(
        TimelineSemaphore const& semaphore,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        std::optional<uint64_t> value = std::nullopt
    ) : semaphore{&semaphore}, stages{stages}, value{value} {}

    std::variant<not_null_ptr<Semaphore const>, not_null_ptr<TimelineSemaphore const>> semaphore;
    VkPipelineStageFlags2 stages;

    /**
     * @brief timeline semaphores only
     * @attention an explicit value does not change the counter of the semaphore
     */
    std::optional<uint64_t> value = std::nullopt;
};

/**
 * @brief one batch of a queue submit
 */
struct SubmitInfo {
    /**
     * @brief the gpu executes them in order
     */
    std::vector<not_null_ptr<PrimaryCmdBuffer const>> cmdBuffers{};

    std::vector<SemaphoreSubmitInfo> waits{};
    std::vector<SemaphoreSubmitInfo> signals{};
};
}
