#pragma once
#include "CthQueueFamily.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/control/CthWaitStage.hpp"
#include "vulkan/utility/cth_constants.hpp"

#include <cstdint>
#include <span>
#include <vector>
#include<cth/cth_pointer.hpp>

#include <vulkan/vulkan.h>



namespace cth::vk {
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


    explicit Queue(QueueFamilyProperties const family_properties) : _familyProperties(family_properties) {}
    ~Queue() = default;

    /**
     * @brief wraps the vulkan queue
     * @note normally called by the device, not the user
     */
    void wrap(uint32_t family_index, uint32_t queue_index, VkQueue vk_queue);


    /**
     * @brief advances and submits the submit_info
     * @param[in, out] submit_info @attention calls @ref submit_info.next()
     */
    void submit(SubmitInfo& submit_info) const;

    /**
     * @brief submits the submit_info
     * @note does not advance the @ref submit_info
     */
    void const_submit(SubmitInfo const& submit_info) const;

    /**
     * @brief skip submits without the command_buffer to advance the sync primitives
     * @param[in, out] submit_info @attention calls @ref submit_info.next()
     * @throws cth::except::vk_result_exception result of vkQueueSubmit()
     */
    void skip(SubmitInfo& submit_info) const;

    /**
     * @brief does an empty submit without the commandBuffer
     * @note this call respects sync primitives
     * @note does not advance the @ref submit_info
     */
    void const_skip(SubmitInfo const& submit_info) const;

    /**
     * @brief presents the image via vkQueuePresentKHR()
     * @param image_index swapchain image index
     * @return result of vkQueuePresentKHR() [VK_SUCCESS, VK_SUBOPTIMAL_KHR, VK_ERROR_OUT_OF_DATE_KHR]
     * @throws cth::except::vk_result_exception result of vkQueuePresentKHR()
     */
    [[nodiscard]] VkResult present(uint32_t image_index, PresentInfo& present_info) const;

    /**
     * @brief skips presenting the info
     * @note this call respects sync primitives
     */
    void const_skip(PresentInfo const& present_info) const;

private:
    void submit(VkSubmitInfo const* submit_info, VkFence fence = nullptr) const;


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

    Queue(Queue const& other) = default;
    Queue(Queue&& other) = default;
    Queue& operator=(Queue const& other) = default;
    Queue& operator=(Queue&& other) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(Queue const* queue);
    static void debug_check_present_queue(Queue const* queue);
#define DEBUG_CHECK_QUEUE(queue_ptr) Queue::debug_check(queue_ptr)
#define DEBUG_CHECK_PRESENT_QUEUE(queue_ptr) Queue::debug_check_present_queue(queue_ptr)

#else

#define DEBUG_CHECK_QUEUE(queue_ptr) ((void)0)
#define DEBUG_CHECK_PRESENT_QUEUE(queue_ptr) ((void)0)
#endif
};

inline size_t submitInfoId = 0; //TEMP remove the id

/**
 * @brief encapsulates the submit info for reuse
 * @note must be destroyed before any of the vulkan structures it references
 * @note added structures may be moved
 * @note //TODO will not work if you have the same semaphore for multiple stages bc not implemented
 *
 */
struct Queue::SubmitInfo {
    SubmitInfo(std::span<PrimaryCmdBuffer const* const> cmd_buffers, std::span<PipelineWaitStage const> wait_stages,
        std::span<BasicSemaphore* const> signal_semaphores, BasicFence const* fence);

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
    void initSignal(std::span<BasicSemaphore* const> signal_semaphores);

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

    BasicFence const* _fence = nullptr;


    size_t _id = submitInfoId++; //TEMP remove this
public:
    [[nodiscard]] VkSubmitInfo const* get() const { return &_submitInfo; }
    [[nodiscard]] VkSubmitInfo const* skip() const { return &_skipSubmitInfo; }
    [[nodiscard]] VkFence fence() const {
        if(_fence == nullptr) return nullptr;
        return _fence->get();
    }
};

struct Queue::PresentInfo {
    /**
     * @param swapchain must not be recreated
     *
     */
    explicit PresentInfo(BasicSwapchain const* swapchain, std::span<BasicSemaphore const*> wait_semaphores);

    void createInfo();

private:
    VkPresentInfoKHR _presentInfo{};
    VkSubmitInfo _skipInfo{};
    std::vector<VkPipelineStageFlags> _skipPipelineStages;

    VkSwapchainKHR _swapchain;
    std::vector<VkSemaphore> _waitSemaphores;

public:
    [[nodiscard]] VkPresentInfoKHR const* create(uint32_t const& image_index);
    [[nodiscard]] VkSubmitInfo const* skip() const { return &_skipInfo; }
};

}
