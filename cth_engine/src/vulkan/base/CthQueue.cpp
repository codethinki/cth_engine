#include "CthQueue.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"

#include <algorithm>
#include<array>
#include <utility>
#include<vector>
#include <cth/cth_log.hpp>

#include "vulkan/surface/CthSwapchain.hpp"

namespace cth {
//TEMP implement this class
using std::vector;



Queue::Queue(Config config) { _config = std::move(config); }

void Queue::wrap(Device* device, const uint32_t queue_index, VkQueue vk_queue) {
    _device = device;
    _queueIndex = queue_index;
    _vkQueue = vk_queue;
}
VkResult Queue::submit(const SubmitInfo& submit_info) const {
    return vkQueueSubmit(_vkQueue, 1, submit_info.get(), submit_info.fence());
}
VkResult Queue::present(const uint32_t image_index, const PresentInfo& present_info) const {
    VkResult result = VK_SUCCESS;

    auto info = present_info.createInfo(&image_index, &result);

    return vkQueuePresentKHR(_vkQueue, &info);
}



} //namespace cth

//SubmitInfo

namespace cth {
Queue::SubmitInfo::SubmitInfo(std::span<const PrimaryCmdBuffer*> cmd_buffers, std::span<const VkPipelineStageFlags> wait_stages) {
    _cmdBuffers.resize(cmd_buffers.size());
    std::ranges::transform(cmd_buffers, _cmdBuffers.begin(), [](const PrimaryCmdBuffer* cmd_buffer) { return cmd_buffer->get(); });

    _waitStages.resize(wait_stages.size());
    std::ranges::copy(wait_stages, _waitStages.begin());

    //TEMP add semaphores

    _submitInfo = createInfo();
}
VkSubmitInfo Queue::SubmitInfo::createInfo() const {
    return VkSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = static_cast<uint32_t>(_waitSemaphores.size()),
        .pWaitSemaphores = _waitSemaphores.data(),
        .pWaitDstStageMask = _waitStages.data(),

        .commandBufferCount = static_cast<uint32_t>(_cmdBuffers.size()),
        .pCommandBuffers = _cmdBuffers.data(),

        .signalSemaphoreCount = static_cast<uint32_t>(_signalSemaphores.size()),
        .pSignalSemaphores = _signalSemaphores.data(),
    };
}



} //namespace cth

//PresentInfo

namespace cth {
    Queue::PresentInfo::PresentInfo(const Swapchain& swapchain) {
        _swapchain = swapchain.get();

        //TEMP add semaphores
    }

    VkPresentInfoKHR Queue::PresentInfo::createInfo(const uint32_t* image_index, VkResult* result) const {
        return VkPresentInfoKHR{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount = 1u,
            .pSwapchains = &_swapchain,
            .pImageIndices = image_index,
            .pResults = result,
        };
    }

}
