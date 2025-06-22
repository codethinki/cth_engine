#pragma once
#include "src/vulkan/render/pass/RenderPassBeginConfig.hpp"
#include "src/vulkan/resource/image/ImageConfig.hpp"

namespace cth::vk {
class RenderPulse;
}

namespace cth::vk {
class Framebuffer;
}

namespace cth::vk {
class PrimaryCmdBuffer;
}

namespace cth::vk {
class AttachmentCollection;
}

namespace cth::vk {
class Subpass;
class Core;
class RenderPass;
class GraphicsCore;

}
//TEMP left off here. the frame resources class should be completed and the only thing left is to delete the stuff from the swapchain

//TEMP this is a temp fix this class is ugly af and should not be like that. create some proper system 
namespace cth {
class FrameResources {
public:
    FrameResources(vk::Core const& core, vk::GraphicsCore const& graphics_core);

    void beginRenderPass(vk::PrimaryCmdBuffer const& cmd_buffer);
    void endRenderPass(vk::PrimaryCmdBuffer const& cmd_buffer) const;

private:
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;
    [[nodiscard]] VkFormat queryDepthFormat() const;

    [[nodiscard]] VkFormat findDepthFormat() const;
    [[nodiscard]] vk::ImageConfig createDepthImageConfig() const;

    void createDepthAttachments();
    void createMsaaAttachments();
    void createAttachments();

    [[nodiscard]] std::unique_ptr<vk::Subpass> createSubpass() const;
    [[nodiscard]] static VkSubpassDependency createSubpassDependency();
    [[nodiscard]] vk::RenderPassBeginConfig createRenderPassBeginConfig() const;

    void createRenderPass();

    void createFramebuffers();

    void create();

    void resize();

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    cth::not_null<vk::Core const*> _core;
    cth::not_null<vk::GraphicsCore const*> _graphicsCore;


    std::unique_ptr<vk::RenderPass> _renderPass;
    std::unique_ptr<vk::Subpass> _subpass;

    std::unique_ptr<vk::AttachmentCollection> _msaaAttachments;
    std::unique_ptr<vk::AttachmentCollection> _depthAttachments;

    std::vector<vk::Framebuffer> _framebuffers;

    [[nodiscard]] vk::Framebuffer const& framebuffer();
public:
};
}
