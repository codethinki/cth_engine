#include "context.hpp"

#include "validation.hpp"

#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/fence.hpp"


namespace cvk::test {
Core::Config const Context::CONFIG{
    .queueProperties = {QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::COMPUTE},
};

Context::Context(Core::Config const& config) : instance{APP_NAME, NO_EXTENSIONS, counting_messenger()},
    core{instance, {}, config} {}

Context::ValidationCheck::ValidationCheck() { reset_validation_errors(); }

Context::ValidationCheck::~ValidationCheck() { EXPECT_EQ(validation_errors(), 0); }

void submit_and_wait(Context const& context, PrimaryCmdBuffer const& primary) {
    Fence const fence{context.core, 0};

    VkCommandBufferSubmitInfo const bufferInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = primary.get(),
        .deviceMask = 0,
    };
    VkSubmitInfo2 const submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = 0,
        .pWaitSemaphoreInfos = nullptr,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &bufferInfo,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos = nullptr,
    };

    context.core.queue(0).raw_submit(submitInfo, fence.get());
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
    uint32_t mipLevel
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
        .srcImageLayout = image.layout(mipLevel),
        .dstBuffer = dst.get(),
        .regionCount = 1,
        .pRegions = &region,
    };
    context.core.functions()->vkCmdCopyImageToBuffer2(cmdBuffer.get(), &info);
}
}
