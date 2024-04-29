#pragma once
#include <cstdint>
#include <span>

#include <vulkan/vulkan.h>

#include "CthDevice.hpp"

namespace cth {
class Swapchain;
class PrimaryCmdBuffer;
}

namespace cth {

class Queue {

public:
    struct Config;
    struct SubmitInfo;
    struct PresentInfo;

    explicit Queue(Config config);
    ~Queue() = default;

    /**
     * \brief wraps the vulkan queue
     * \note normally called by the device, not the user
     */
    void wrap(Device* device, uint32_t queue_index, VkQueue vk_queue);

    [[nodiscard]] VkResult submit(const SubmitInfo& submit_info) const;

    VkResult present(const uint32_t image_index, const PresentInfo& present_info) const;

    struct Config {
        uint32_t familyIndex;
        VkQueueFlags familyProperties;
    };

private:
    Config _config;

    Device* _device = nullptr;
    uint32_t _queueIndex = 0;
    VkQueue _vkQueue = VK_NULL_HANDLE;

public:
    [[nodiscard]] auto valid() const { return _device != nullptr && _vkQueue != VK_NULL_HANDLE; }
    [[nodiscard]] auto get() const { return _vkQueue; }
    [[nodiscard]] auto index() const { return _queueIndex; }

    [[nodiscard]] auto familyIndex() const { return _config.familyIndex; }
    [[nodiscard]] auto familyProperties() const { return _config.familyProperties; }
};


/**
 * \brief encapsulates the submit info for reuse
 * \note must be destroyed before any of the vulkan structures it references
 * \note added structures may be moved
 */
struct Queue::SubmitInfo {
    SubmitInfo(std::span<const PrimaryCmdBuffer*> cmd_buffers, std::span<const VkPipelineStageFlags> wait_stages);
    ~SubmitInfo() = delete; //TEMP add the semaphores to the constructor



private:
    [[nodiscard]] VkSubmitInfo createInfo() const;

    VkSubmitInfo _submitInfo;

    std::vector<VkCommandBuffer> _cmdBuffers;
    std::vector<VkSemaphore> _waitSemaphores;
    std::vector<VkSemaphore> _signalSemaphores;
    std::vector<VkPipelineStageFlags> _waitStages;
    VkFence _fence = VK_NULL_HANDLE;

public:
    [[nodiscard]] const auto* get() const { return &_submitInfo; }
    [[nodiscard]] auto fence() const { return _fence; }
};

struct Queue::PresentInfo {
    PresentInfo(const Swapchain& swapchain); 
    ~PresentInfo() = delete; //TEMP add wait semaphores

    [[nodiscard]] VkPresentInfoKHR createInfo(const uint32_t* image_index, VkResult* result) const;
private:
    VkSwapchainKHR _swapchain;
    std::vector<VkSemaphore> _waitSemaphores;
};

}
