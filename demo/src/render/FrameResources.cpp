#include "FrameResources.hpp"

#include "src/vulkan/base/CthCore.hpp"
#include "src/vulkan/base/CthPhysicalDevice.hpp"
#include "src/vulkan/render/cmd/CthCmdBuffer.hpp"
#include "src/vulkan/render/pass/AttachmentCollection.hpp"
#include "src/vulkan/render/pass/CthRenderPass.hpp"
#include "src/vulkan/render/pass/CthSubpass.hpp"
#include "src/vulkan/render/pass/RenderPassConfig.hpp"
#include "src/vulkan/resource/framebuffer/ScFramebufferCollection.hpp"
#include "src/vulkan/surface/graphics_core/CthGraphicsCore.hpp"
#include "src/vulkan/surface/swapchain/CthBasicSwapchain.hpp"
#include "src/vulkan/utility/cth_vk_overloads.hpp"

namespace cth {


FrameResources::FrameResources(vk::Core const& core, vk::GraphicsCore const& graphics_core) : _core{&core}, _graphicsCore{&graphics_core} {}
FrameResources::~FrameResources() = default;
void FrameResources::beginRenderPass(vk::PrimaryCmdBuffer const& cmd_buffer) const {
    _renderPass->begin(cmd_buffer, 0, framebuffer());

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
void FrameResources::endRenderPass(vk::PrimaryCmdBuffer const& cmd_buffer) const { _renderPass->end(cmd_buffer); }

VkSampleCountFlagBits FrameResources::evalMsaaSampleCount() const {
    uint32_t const maxSamples = _core->physicalDevice().maxSampleCount() / 2; //TODO add proper max_sample_count selection

    uint32_t samples = 1;
    while(samples < maxSamples && samples < vk::constants::MAX_MSAA_SAMPLES) samples *= 2;

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
vk::ImageConfig FrameResources::createDepthImageConfig() const {
    auto const depthFormat = findDepthFormat();


    return vk::ImageConfig{
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .format = depthFormat,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .samples = _msaaSamples,
    };
}

void FrameResources::createDepthAttachments() {
    vk::AttachmentDescription description{
        .samples = _msaaSamples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .referenceLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    auto const imageConfig = createDepthImageConfig();

    _depthAttachments = std::make_unique<vk::AttachmentCollection>(*_core, vk::constants::FRAMES_IN_FLIGHT, 2, imageConfig, description,
        _graphicsCore->swapchainExtent());
}

void FrameResources::createMsaaAttachments() {
    vk::AttachmentDescription description{
        .samples = _msaaSamples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .referenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };


    vk::ImageConfig imageConfig{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = _graphicsCore->swapchainImageFormat(),
        .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .samples = _msaaSamples,
    };

    _msaaAttachments = std::make_unique<vk::AttachmentCollection>(*_core, vk::constants::FRAMES_IN_FLIGHT, 1, imageConfig, description,
        _graphicsCore->swapchainExtent());
}

void FrameResources::createAttachments() {
    createMsaaAttachments();
    createDepthAttachments();
}


std::unique_ptr<vk::Subpass> FrameResources::createSubpass() const {
    return std::make_unique<vk::Subpass>(
        0u,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        std::vector<vk::AttachmentCollection*>{},
        std::vector{_msaaAttachments.get()},
        std::vector{_graphicsCore->swapchainResolveAttachments()},
        _depthAttachments.get(),
        std::vector<vk::AttachmentCollection*>{}
    );
}

VkSubpassDependency FrameResources::createSubpassDependency() {
    VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    vk::BasicSwapchain::addResolveSubpassDependencyFlags(dependency);
    return dependency;
}

vk::RenderPassBeginConfig FrameResources::createRenderPassBeginConfig() const {
    return {
        .clearValues = {{
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

    _renderPass = std::make_unique<vk::RenderPass>(
        *_core,
        vk::RenderPass::Config{
            .subpasses{_subpass.get()},
            .dependencies{subpassDependency},
            .beginConfigs{beginConfig}
        }
    );
}
void FrameResources::createFramebufferCollection() {
    _framebufferCollection = std::make_unique<vk::ScFramebufferCollection>(*_core, *_graphicsCore->swapchain(), *_renderPass,
        vk::FramebufferCollectionConfig::Attachments(std::vector{_msaaAttachments.get(), _depthAttachments.get()})
    );
}


void FrameResources::create() {
    _msaaSamples = evalMsaaSampleCount();

    createAttachments();

    createRenderPass();
    createFramebufferCollection();
}
void FrameResources::resize() const {
    auto const extent = _graphicsCore->swapchainExtent();

    CTH_CRITICAL(extent == (VkExtent2D{0, 0}), "extent must not be empty") {}

    _depthAttachments->create(extent);
    _msaaAttachments->create(extent);

    _framebufferCollection->create(extent);
}
vk::Framebuffer const& FrameResources::framebuffer() const {
    auto const& pulse = _graphicsCore->renderPulse();

    return _framebufferCollection->get(pulse.get());
}
vk::RenderPass const& FrameResources::renderPass() const { return *_renderPass; }


}
