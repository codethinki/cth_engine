#include "FrameResources.hpp"

#include "jolly/utility/CthOSWindow.hpp"
#include "jolly/utility/GraphicsCore.hpp"
#include "jvk/base/core.hpp"
#include "jvk/base/physical_device.hpp"
#include "jvk/render/cmd/cmd_buffer.hpp"
#include "jvk/render/pass/subpass.hpp"
#include "jvk/render/pass/render_pass.hpp"
#include "jvk/render/pass/render_pass_config.hpp"
#include "jvk/render/pass/attachment/attachment_collection.hpp"
#include "jvk/render/pass/attachment/attachment_description.hpp"
#include "jvk/render/pass/framebuffer/swapchain_fb_collection.hpp"
#include "jvk/surface/swapchain/swapchain.hpp"
#include "jvk/utility/vk_overloads.hpp"

namespace cth {


FrameResources::FrameResources(jvk::Core const& core, Config config
)
    : _core{&core}
    ,
    _config{std::move(config)} {
    create();
}

FrameResources::~FrameResources() = default;

void FrameResources::beginRenderPass(jvk::PrimaryCmdBuffer const& cmd_buffer) const {
    _renderPass->begin(cmd_buffer, framebuffer());

    auto const extent = _graphicsCore->swapchainExtent();

    VkViewport const viewport{
        .x = 0,
        .y = 0,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0,
        .maxDepth = 1.0f,
    };
    VkRect2D const scissor{
        .offset = {0, 0},
        .extent = extent
    };
    _core->functions()->vkCmdSetViewport(cmd_buffer.get(), 0, 1, &viewport);
    _core->functions()->vkCmdSetScissor(cmd_buffer.get(), 0, 1, &scissor);
}

void FrameResources::endRenderPass(jvk::PrimaryCmdBuffer const& cmd_buffer) const {
    _renderPass->end(cmd_buffer);
}


void FrameResources::resize() const {
    auto const extent = _graphicsCore->swapchainExtent();

    CTH_CRITICAL(extent == (VkExtent2D{0, 0}), "extent must not be empty") {}

    _depthAttachments->create(extent);
    _msaaAttachments->create(extent);

    _framebufferCollection->create(extent);
    _renderPass->resize(extent);
}

void FrameResources::acquireFrame() const {
    _graphicsCore->acquireFrame();
}

void FrameResources::skipAcquire() const {
    _graphicsCore->skipAcquire();
}

bool FrameResources::presentFrame() const {
    return _graphicsCore->presentFrame();
}

void FrameResources::skipPresent() const {
    _graphicsCore->skipPresent();
}

VkSampleCountFlagBits FrameResources::evalMsaaSampleCount() const {
    uint32_t const maxSamples = _core->physicalDevice().maxSampleCount() / 2;
    //TODO add proper max_sample_count selection

    uint32_t samples = 1;
    while(samples < maxSamples && samples < jvk::constants::MAX_MSAA_SAMPLES) samples *= 2;

    return static_cast<VkSampleCountFlagBits>(samples);
}

VkFormat FrameResources::findDepthFormat() const {
    auto const format = _core->physicalDevice().findSupportedFormat(
        std::vector{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );

    CTH_STABLE_ERR(format == VK_FORMAT_UNDEFINED, "depth format must not be VK_FORMAT_UNDEFINED")
        throw details->exception();

    return format;
}

void FrameResources::createDepthAttachments() {
    auto const description = jvk::AttachmentDescription::DepthBuffer(RENDER_SUBPASS_INDEX, _msaaSamples);
    auto const imageConfig = jvk::ImageConfig::DepthBuffer(findDepthFormat(), _msaaSamples);

    _depthAttachments = std::make_unique<jvk::AttachmentCollection>(
        *_core,
        jvk::AttachmentCollection::Config{jvk::constants::FRAMES_IN_FLIGHT, 2, imageConfig, description},
        _graphicsCore->swapchainExtent()
    );
}

void FrameResources::createMsaaAttachments() {
    auto const description = jvk::AttachmentDescription::Color(RENDER_SUBPASS_INDEX, _msaaSamples);


    jvk::ImageConfig const imageConfig{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = _graphicsCore->swapchainImageFormat(),
        .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .samples = _msaaSamples,
    };

    _msaaAttachments = std::make_unique<jvk::AttachmentCollection>(
        *_core,
        jvk::AttachmentCollection::Config{jvk::constants::FRAMES_IN_FLIGHT, 1, imageConfig, description},
        _graphicsCore->swapchainExtent());
}


void FrameResources::createAttachments() {
    createMsaaAttachments();
    createDepthAttachments();
}

std::unique_ptr<jvk::Subpass> FrameResources::createSubpass() const {
    return std::make_unique<jvk::Subpass>(
        RENDER_SUBPASS_INDEX,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        std::vector{_msaaAttachments.get()},
        std::vector{_graphicsCore->swapchainResolveAttachments()},
        _depthAttachments.get()
    );
}

VkSubpassDependency FrameResources::createSubpassDependency() {
    VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = RENDER_SUBPASS_INDEX,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    jvk::Swapchain::addResolveSubpassDependencyFlags(dependency);
    return dependency;
}

jvk::RenderPassBeginConfig FrameResources::createRenderPassBeginConfig() const {
    return {
        .clearValues = {{
            {.color = {{0, 0, 0, 1}}},
            //TEMP this is the swapchain clear value and should be set by the swapchain not manually
            {.color = {{0, 0, 0, 1}}},
            {.depthStencil = {1.0f, 0}}
        }},
        .extent = _graphicsCore->swapchainExtent()
    };
}

void FrameResources::createRenderPass() {
    _subpass = createSubpass();
    auto const subpassDependency = createSubpassDependency();
    auto const beginConfig = createRenderPassBeginConfig();

    _renderPass = std::make_unique<jvk::RenderPass>(
        *_core,
        jvk::RenderPass::Config{
            .subpasses{_subpass.get()},
            .dependencies{subpassDependency},
            .beginConfig{beginConfig}
        },
        jvk::create
    );
}


void FrameResources::createFramebufferCollection() {
    _framebufferCollection = std::make_unique<jvk::ScFramebufferCollection>(
        *_core,
        *_graphicsCore->swapchain(),
        *_renderPass,
        jvk::FramebufferCollectionConfig::Attachments(
            std::vector{
                _msaaAttachments.get(),
                _depthAttachments.get()
            }
        ),
        jvk::create
    );
}

void FrameResources::createGraphicsCore() {
    _graphicsCore = std::make_unique<jly::GraphicsCore>(
        *_core,
        jly::GraphicsCore::Config{
            .subpassConfig = jvk::SwapchainSubpassConfig::ColorAttachmentOptimal(RENDER_SUBPASS_INDEX),
        },
        _config.windowName,
        _config.windowExtent,
        _config.presentQueue
    );
}

void FrameResources::create() {
    createGraphicsCore();

    _msaaSamples = evalMsaaSampleCount();

    createAttachments();

    createRenderPass();
    createFramebufferCollection();
}


jvk::Framebuffer const& FrameResources::framebuffer() const {
    auto const& pulse = _graphicsCore->renderPulse();
    return _framebufferCollection->get(pulse.get());
}

jvk::RenderPass const& FrameResources::renderPass() const { return *_renderPass; }

bool FrameResources::shouldClose() const {
    return _graphicsCore->osWindow()->shouldClose();
}

jly::RenderPulse const& FrameResources::renderPulse() const {
    return _graphicsCore->renderPulse();
}

jly::GraphicsSyncConfig const& FrameResources::syncConfig() const {
    return *_graphicsCore->syncConfig();
}

}
