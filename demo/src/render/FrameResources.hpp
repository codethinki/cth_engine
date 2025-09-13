#pragma once
#include "frame_resources_config.hpp"

#include "jvk/render/pass/render_pass_begin_config.hpp"


namespace jly {
class RenderPulse;
class GraphicsSyncConfig;
class GraphicsCore;
}

namespace jvk {
class ScFramebufferCollection;
class Subpass;
class Core;
class RenderPass;
class AttachmentCollection;
class Framebuffer;
class PrimaryCmdBuffer;
}

//TEMP this is a temp fix this class is ugly af and should not be like that. create some proper system
namespace cth {
class FrameResources {
    static cxpr uint32_t RENDER_SUBPASS_INDEX = 0;

public:
    using Config = FrameResourcesConfig;

    FrameResources(jvk::Core const& core, Config config
        );

    ~FrameResources();

    void beginRenderPass(jvk::PrimaryCmdBuffer const& cmd_buffer) const;

    void endRenderPass(jvk::PrimaryCmdBuffer const& cmd_buffer) const;

    void resize() const;

    void acquireFrame() const;
    void skipAcquire() const;

    [[nodiscard]] bool presentFrame() const;
    void skipPresent() const;

private:
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;

    [[nodiscard]] VkFormat findDepthFormat() const;

    void createDepthAttachments();

    void createMsaaAttachments();

    void createAttachments();

    [[nodiscard]] std::unique_ptr<jvk::Subpass> createSubpass() const;

    [[nodiscard]] static VkSubpassDependency createSubpassDependency();

    [[nodiscard]] jvk::RenderPassBeginConfig createRenderPassBeginConfig() const;

    void createRenderPass();

    void createFramebufferCollection();

    void createGraphicsCore();

    void create();

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    not_null<jvk::Core const*>
    _core;
    Config _config;

    std::unique_ptr<jly::GraphicsCore> _graphicsCore;
    std::unique_ptr<jvk::RenderPass> _renderPass;
    std::unique_ptr<jvk::Subpass> _subpass;

    std::unique_ptr<jvk::AttachmentCollection> _msaaAttachments;
    std::unique_ptr<jvk::AttachmentCollection> _depthAttachments;

    std::unique_ptr<jvk::ScFramebufferCollection> _framebufferCollection;


    [[nodiscard]] jvk::Framebuffer const& framebuffer() const;
    [[nodiscard]] VkExtent2D swapchainExtent() const;

public:
    [[nodiscard]] jvk::RenderPass const& renderPass() const;
    [[nodiscard]] auto const& graphicsCore() const { return *_graphicsCore; }
    [[nodiscard]] bool shouldClose() const;
    [[nodiscard]] jly::RenderPulse const& renderPulse() const;
    [[nodiscard]] jly::GraphicsSyncConfig const& syncConfig() const;
    [[nodiscard]] auto const& core() const { return *_graphicsCore; }
    [[nodiscard]] VkSampleCountFlagBits msaaSampleCount() const { return _msaaSamples; }
};
}