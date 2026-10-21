#include "context.hpp"

#include "validation.hpp"

#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/fence.hpp"

#include <optional>
#include <span>


namespace cvk::test {
Core::Config const Context::CONFIG{
    .queueProperties = {QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::COMPUTE},
};

struct Context::Owned {
    explicit Owned(Core::Config const& config) : instance{instance_config(), create, counting_messenger()},
        core{instance, {}, config} {}

    Instance instance;
    Core core;
};

Core::Config const PresentContext::CONFIG{
    .queueProperties = {QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::PRESENT},
};

namespace {
    /**
     * @brief one instance for both shared contexts, the cores and the surface come at their first use
     * @details volk holds the functions of one instance, cvk assumes one instance per process
     */
    struct SharedContexts {
        Instance instance{instance_config(headless_extensions()), create, counting_messenger()};
        std::optional<Core> core{};
        std::optional<Surface> surface{};
        std::optional<Core> presentCore{};
    };

    std::optional<SharedContexts> shared{};

    SharedContexts& shared_contexts() {
        if(!shared.has_value()) shared.emplace();
        // a test with an own instance loaded its functions into volk
        volkLoadInstanceOnly(shared->instance.get());
        return *shared;
    }

    Core const& shared_core() {
        auto& contexts = shared_contexts();
        if(!contexts.core.has_value())
            contexts.core.emplace(contexts.instance, std::span<Surface const>{}, Context::CONFIG);
        return *contexts.core;
    }

    Surface const& shared_surface() {
        auto& contexts = shared_contexts();
        if(!contexts.surface.has_value()) contexts.surface.emplace(headless_surface(contexts.instance));
        return *contexts.surface;
    }

    Core const& shared_present_core() {
        auto& contexts = shared_contexts();
        if(!contexts.presentCore.has_value()) {
            contexts.presentCore.emplace(
                contexts.instance,
                std::span{&shared_surface(), 1},
                PresentContext::CONFIG
            );
        }
        return *contexts.presentCore;
    }
}

Context::Context() : instance{shared_contexts().instance}, core{shared_core()} {}

Context::Context(Core::Config const& config) : _owned{std::make_unique<Owned>(config)},
    instance{_owned->instance}, core{_owned->core} {}

Context::~Context() = default;

PresentContext::PresentContext() : instance{shared_contexts().instance}, core{shared_present_core()},
    _surface{shared_surface()} {}

void destroy_shared_contexts() {
    if(!shared.has_value()) return;

    volkLoadInstanceOnly(shared->instance.get());
    reset_validation_errors();
    shared.reset();
    EXPECT_EQ(validation_errors(), 0) << "errors at the destruction of the shared contexts, e.g. leaks";
}

Context::ValidationCheck::ValidationCheck() { reset_validation_errors(); }

Context::ValidationCheck::~ValidationCheck() { EXPECT_EQ(validation_errors(), 0); }

void submit_and_wait(Context const& context, PrimaryCmdBuffer const& primary) {
    Fence const fence{context.core, 0};

    context.core.queue(0).submit({.cmdBuffers = {&primary}}, &fence);
    fence.wait();
}

void host_read_barrier(Context const& context, BaseCmdBuffer const& cmdBuffer) {
    VkMemoryBarrier2 const barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
    };
    VkDependencyInfo const dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 0,
        .pImageMemoryBarriers = nullptr,
    };
    context.core.functions()->vkCmdPipelineBarrier2(cmdBuffer.get(), &dependency);
}

void copy_to_buffer(
    Context const& context,
    BaseCmdBuffer const& cmdBuffer,
    Image const& image,
    BaseBuffer const& dst,
    size_t dstOffset,
    uint32_t mipLevel,
    VkImageLayout layout
) {
    VkBufferImageCopy2 const region{
        .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
        .pNext = nullptr,
        .bufferOffset = dstOffset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0, 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {image.extent().width >> mipLevel, image.extent().height >> mipLevel, 1},
    };
    VkCopyImageToBufferInfo2 const info{
        .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
        .pNext = nullptr,
        .srcImage = image.get(),
        .srcImageLayout = layout,
        .dstBuffer = dst.get(),
        .regionCount = 1,
        .pRegions = &region,
    };
    context.core.functions()->vkCmdCopyImageToBuffer2(cmdBuffer.get(), &info);
}
}
