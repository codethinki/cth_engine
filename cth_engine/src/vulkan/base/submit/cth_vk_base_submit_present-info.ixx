module;
#include "lib/volk.hpp"

export module cth.vk.base.submit.present_info;

import std;

namespace cth::vk {
class BasicSwapchain;
class Semaphore;
}

export namespace cth::vk {


struct PresentInfo {
    /**
     * @param swapchain must not be recreated
     */
    explicit PresentInfo(BasicSwapchain const* swapchain, std::span<Semaphore const*> wait_semaphores);

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