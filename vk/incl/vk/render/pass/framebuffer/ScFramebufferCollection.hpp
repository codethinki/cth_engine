#pragma once
#include "FramebufferCollection.hpp"


namespace cth::vk {

class ScFramebufferCollection : public FramebufferCollection {
public:
    /**
     * @brief constructs
     * @param swapchain image index and attachment source
     * @param render_pass swapchain attachment index must be at 0
     */
    ScFramebufferCollection(Core const& core, Swapchain const& swapchain, RenderPass const& render_pass,
        FramebufferCollectionConfig const& config);
    /**
     * @brief constructs and creates
     * @param swapchain image index and attachment source
     * @param render_pass swapchain attachment index must be at 0
     * @details calls:
     *          - ScFramebufferCollection(Core const&, BasicSwapchain const&, RenderPass const&, FramebufferCollectionConfig const&)
     *          - @ref create(VkExtent2D)
     */
    ScFramebufferCollection(Core const& core, Swapchain const& swapchain, RenderPass const& render_pass,
        FramebufferCollectionConfig const& config, create_t);

    void create();
    void create(VkExtent2D framebuffer_extent) override;


private:
    void swapchainResized();
    void create(VkExtent2D extent, bool reconfigure);


    static auto addSwapchainAttachments(Swapchain const& swapchain, FramebufferCollectionConfig const& config) -> FramebufferCollectionConfig;



    cth::not_null<Swapchain const*> _swapchain;
    size_t _scImages = 0;

public:
    [[nodiscard]] size_t size() const override;
    [[nodiscard]] Framebuffer const& get(size_t pulse_index) const override;
    [[nodiscard]] Framebuffer const& get(size_t swapchain_image_index, size_t pulse_index) const;
};
}
