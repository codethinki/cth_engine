#pragma once
#include "FramebufferCollection.hpp"


namespace cth::vk {

class ScFramebufferCollection : public FramebufferCollection {
public:
    /**
     * @param swapchain image index and attachment source
     * @param render_pass swapchain attachment index must be at 0
     */
    ScFramebufferCollection(Core const& core, BasicSwapchain const& swapchain, RenderPass const& render_pass,
        FramebufferCollectionConfig const& config);

private:
    static auto addSwapchainAttachments(BasicSwapchain const& swapchain, FramebufferCollectionConfig const& config) -> FramebufferCollectionConfig;



    cth::not_null<BasicSwapchain const*> _swapchain;

public:
    [[nodiscard]] size_t size() const override;
    [[nodiscard]] Framebuffer const& get(size_t pulse_index) const override;
};
}
