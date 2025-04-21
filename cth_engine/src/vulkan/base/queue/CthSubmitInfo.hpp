#pragma once
#include <volk.h>

#include <memory>
#include <span>
#include <vector>

namespace cth::vk {
class Fence;
struct PipelineWaitStage;
class PrimaryCmdBuffer;
class Semaphore;
class TimelineSemaphore;



/**
 * @brief encapsulates the submit info for reuse
 * @note must be destroyed before any of the vulkan structures it references
 * @note //TODO will not work if you have the same semaphore for multiple stages bc not implemented
 *
 */
struct SubmitInfo {
    SubmitInfo(std::span<PrimaryCmdBuffer const* const> cmd_buffers, std::span<PipelineWaitStage const> wait_stages,
        std::span<Semaphore* const> signal_semaphores, Fence const* fence);

    /**
     * @brief advances the timeline semaphores and returns this
     */
    [[nodiscard]] SubmitInfo& next();

private:
    void createTimelineInfo();
    void createInfo();
    void createSkipSubmitInfo();
    void create();

    void initWait(std::span<PipelineWaitStage const> wait_stages);
    void initSignal(std::span<Semaphore* const> signal_semaphores);

    VkSubmitInfo _submitInfo{};
    VkSubmitInfo _skipSubmitInfo{};
    std::unique_ptr<VkTimelineSemaphoreSubmitInfo> _timelineInfo{};



    std::vector<VkCommandBuffer> _cmdBuffers;
    std::vector<VkSemaphore> _waitSemaphores;
    std::vector<VkSemaphore> _signalSemaphores;
    std::vector<VkPipelineStageFlags> _pipelineWaitStages;

    std::vector<TimelineSemaphore const*> _waitTimelineSemaphores;
    std::vector<TimelineSemaphore*> _signalTimelineSemaphores;
    std::vector<size_t> _waitValues{};
    std::vector<size_t> _signalValues{};

    Fence const* _fence = nullptr;

public:
    [[nodiscard]] VkSubmitInfo const* get() const { return &_submitInfo; }
    [[nodiscard]] VkSubmitInfo const* skip() const { return &_skipSubmitInfo; }
    [[nodiscard]] VkFence fence() const;
};
}