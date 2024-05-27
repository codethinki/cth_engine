#pragma once
#include "CthQueueFamily.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/control/CthWaitStage.hpp"
#include "vulkan/utility/CthConstants.hpp"

#include <cstdint>
#include <span>
#include <vector>
#include <cth/cth_memory.hpp>

#include <vulkan/vulkan.h>





namespace cth {
class TimelineSemaphore;
class BasicSemaphore;
class BasicFence;
class BasicCore;
class BasicSwapchain;
class PrimaryCmdBuffer;



class Queue {
public:
    struct Config;
    struct SubmitInfo;
    struct TimelineSubmitInfo;
    struct PresentInfo;


    explicit Queue(const QueueFamilyProperties family_properties) : _familyProperties(family_properties) {}
    ~Queue() = default;

    /**
     * \brief wraps the vulkan queue
     * \note normally called by the device, not the user
     */
    void wrap(uint32_t family_index, uint32_t queue_index, VkQueue vk_queue);

    [[nodiscard]] VkResult submit(const SubmitInfo& submit_info) const;
    [[nodiscard]] VkResult present(uint32_t image_index, const PresentInfo& present_info) const;

private:
    QueueFamilyProperties _familyProperties;

    uint32_t _familyIndex = 0;
    uint32_t _queueIndex = 0;
    VkQueue _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] auto valid() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] auto get() const { return _handle; }
    [[nodiscard]] auto index() const { return _queueIndex; }

    [[nodiscard]] auto familyIndex() const { return _familyIndex; }
    [[nodiscard]] auto familyProperties() const { return _familyProperties; }

    Queue(const Queue& other) = default;
    Queue(Queue&& other) = default;
    Queue& operator=(const Queue& other) = default;
    Queue& operator=(Queue&& other) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const Queue* queue);
    static void debug_check_present_queue(const Queue* queue);
#define DEBUG_CHECK_QUEUE(queue_ptr) Queue::debug_check(queue_ptr)
#define DEBUG_CHECK_PRESENT_QUEUE(queue_ptr) Queue::debug_check_present_queue(queue_ptr)

#else

#define DEBUG_CHECK_QUEUE(queue_ptr) ((void)0)
#define DEBUG_CHECK_PRESENT_QUEUE(queue_ptr) ((void)0)
#endif
};

/**
 * \brief encapsulates the submit info for reuse
 * \note must be destroyed before any of the vulkan structures it references
 * \note added structures may be moved
 */
struct Queue::SubmitInfo {
    SubmitInfo(std::span<const PrimaryCmdBuffer* const> cmd_buffers, std::span<const PipelineWaitStage> wait_stages,
        std::span<BasicSemaphore* const> signal_semaphores, const BasicFence* fence);

    [[nodiscard]] const VkSubmitInfo* next();
    //TEMP complete this
private:
    [[nodiscard]] VkSubmitInfo createInfo() const;
    [[nodiscard]] VkTimelineSemaphoreSubmitInfo createTimelineInfo() const;
    void initSignal(std::span<BasicSemaphore* const> signal_semaphores);
    void initWait(std::span<const PipelineWaitStage> wait_stages);

    VkSubmitInfo _submitInfo{};
    VkTimelineSemaphoreSubmitInfo _timelineInfo{};



    std::vector<VkCommandBuffer> _cmdBuffers;
    std::vector<VkSemaphore> _waitSemaphores;
    std::vector<VkSemaphore> _signalSemaphores;
    std::vector<VkPipelineStageFlags> _pipelineWaitStages;

    std::vector<const TimelineSemaphore*> _waitTimelineSemaphores;
    std::vector<TimelineSemaphore*> _signalTimelineSemaphores;
    std::vector<size_t> _waitValues{};
    std::vector<size_t> _signalValues{};

    const BasicFence* _fence = VK_NULL_HANDLE;

public:
    [[nodiscard]] const auto* get() const { return &_submitInfo; }
    [[nodiscard]] VkFence fence() const { return _fence->get(); }
};

struct Queue::PresentInfo {
    explicit PresentInfo(const BasicSwapchain& swapchain, std::span<const BasicSemaphore*> wait_semaphores);

    [[nodiscard]] VkPresentInfoKHR createInfo(const uint32_t* image_index, VkResult* result) const;

private:
    VkSwapchainKHR _swapchain;
    std::vector<VkSemaphore> _waitSemaphores;
};

}
