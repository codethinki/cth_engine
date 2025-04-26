module;
#include "lib/volk.hpp"

export module cth.vk.submit.present_info;

import cth.vk.render.sync.semaphore;
import cth.vk.render.sync.timeline_semaphore;
import cth.vk.util.types;

import std;

export namespace cth::vk {

struct PresentInfo {
    /**
     * @param swapchain must not be recreated
     */
    explicit PresentInfo(vk::not_null<VkSwapchainKHR> vk_swapchain, std::span<Semaphore const*> wait_semaphores);

    void createInfo();

private:
    vk::not_null<VkSwapchainKHR> _vkSwapchain;

    VkPresentInfoKHR _presentInfo{};
    VkSubmitInfo _skipInfo{};
    std::vector<VkPipelineStageFlags> _skipPipelineStages;

    std::vector<VkSemaphore> _waitSemaphores;

public:
    [[nodiscard]] VkPresentInfoKHR const* create(uint32_t const& image_index);
    [[nodiscard]] VkSubmitInfo const* skip() const { return &_skipInfo; }
};

}