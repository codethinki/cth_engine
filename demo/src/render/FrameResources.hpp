#pragma once
#include "src/vulkan/render/pass/RenderPassBeginConfig.hpp"
#include "src/vulkan/resource/image/ImageConfig.hpp"


namespace cth::vk {
class ScFramebufferCollection;
class Subpass;
class Core;
class RenderPass;
class GraphicsCore;
class AttachmentCollection;
class Framebuffer;
class RenderPulse;
class PrimaryCmdBuffer;
}

//TEMP left off here. the frame resources class should be completed and the only thing left is to delete the stuff from the swapchain

//TEMP this is a temp fix this class is ugly af and should not be like that. create some proper system 
namespace cth {
class FrameResources {
public:
    FrameResources(vk::Core const& core, vk::GraphicsCore const& graphics_core);
    ~FrameResources();

    void beginRenderPass(vk::PrimaryCmdBuffer const& cmd_buffer) const;
    void endRenderPass(vk::PrimaryCmdBuffer const& cmd_buffer) const;

    void resize() const;

private:
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;

    [[nodiscard]] VkFormat findDepthFormat() const;
    [[nodiscard]] vk::ImageConfig createDepthImageConfig() const;

    void createDepthAttachments();
    void createMsaaAttachments();
    void createAttachments();

    [[nodiscard]] std::unique_ptr<vk::Subpass> createSubpass() const;
    [[nodiscard]] static VkSubpassDependency createSubpassDependency();
    [[nodiscard]] vk::RenderPassBeginConfig createRenderPassBeginConfig() const;

    void createRenderPass();

    void createFramebufferCollection();

    void create();

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    cth::not_null<vk::Core const*> _core;
    cth::not_null<vk::GraphicsCore const*> _graphicsCore;


    std::unique_ptr<vk::RenderPass> _renderPass;
    std::unique_ptr<vk::Subpass> _subpass;

    std::unique_ptr<vk::AttachmentCollection> _msaaAttachments;
    std::unique_ptr<vk::AttachmentCollection> _depthAttachments;

    std::unique_ptr<vk::ScFramebufferCollection> _framebufferCollection;

    [[nodiscard]] vk::Framebuffer const& framebuffer() const;

public:
    [[nodiscard]] vk::RenderPass const& renderPass() const;
};
}
