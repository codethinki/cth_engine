#include "CthPresentInfo.hpp"

#include "vulkan/render/control/CthTimelineSemaphore.hpp"
#include "vulkan/surface/swapchain/CthBasicSwapchain.hpp"
#include <vulkan/render/control/CthSemaphore.hpp>

#include <volk.h>
#include <cth/io/log.hpp>

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <span>


namespace cth::vk {
PresentInfo::PresentInfo(BasicSwapchain const* swapchain, std::span<Semaphore const*> wait_semaphores) : _swapchain(swapchain->get()) {
    _waitSemaphores.resize(wait_semaphores.size());

    for(auto [dst, src] : std::views::zip(_waitSemaphores, wait_semaphores)) {
        dst = src->get();
        CTH_CRITICAL(dynamic_cast<TimelineSemaphore const*>(src) != nullptr, "semaphores in present info must not be timeline semaphores");
    }

    createInfo();
}
void PresentInfo::createInfo() {
    _skipPipelineStages.resize(_waitSemaphores.size());
    std::ranges::fill(_skipPipelineStages, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    _presentInfo = VkPresentInfoKHR{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = static_cast<uint32_t>(_waitSemaphores.size()),
        .pWaitSemaphores = _waitSemaphores.data(),
        .swapchainCount = 1u,
        .pSwapchains = &_swapchain,
        .pImageIndices = nullptr,
        .pResults = nullptr,
    };
    _skipInfo = VkSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = static_cast<uint32_t>(_waitSemaphores.size()),
        .pWaitSemaphores = _waitSemaphores.data(),
        .pWaitDstStageMask = _skipPipelineStages.data(),
        .commandBufferCount = 0,
    };
}
VkPresentInfoKHR const* PresentInfo::create(uint32_t const& image_index) {
    _presentInfo.pImageIndices = &image_index;
    return &_presentInfo;
}

}
