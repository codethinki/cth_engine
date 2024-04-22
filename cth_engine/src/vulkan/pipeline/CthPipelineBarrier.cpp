#include "CthPipelineBarrier.hpp"

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/resource/buffer/CthBasicBuffer.hpp"
#include "vulkan/resource/image/CthBasicImage.hpp"
#include "vulkan/resource/image/CthImage.hpp"

//ImageBarrier

namespace cth {

ImageBarrier::ImageBarrier(const PipelineStages stages, const unordered_map<BasicImage*, ImageBarrier::Info>& images) : PipelineStages(stages) {
    init(images);
}
ImageBarrier::ImageBarrier(const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage,
    const unordered_map<BasicImage*, ImageBarrier::Info>& images) : PipelineStages{src_stage, dst_stage} { init(images); }

void ImageBarrier::add(BasicImage* image, const Info& info) {
    imageBarriers.emplace_back(
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        nullptr,
        info.src.accessMask,
        info.dst.accessMask,
        image->layout(),
        info.newLayout == VK_IMAGE_LAYOUT_UNDEFINED ? image->layout() : info.newLayout,
        info.src.queueFamilyIndex,
        info.dst.queueFamilyIndex,
        image->get(),
        VkImageSubresourceRange{
            static_cast<VkImageAspectFlags>(info.aspectMask == VK_IMAGE_ASPECT_NONE ? image->aspectMask() : info.aspectMask),
            info.firstMipLevel,
            info.levels == 0 ? image->mipLevels() - info.firstMipLevel : info.levels,
            0, 1
        }
        );
    images.push_back(image);
    if(info.newLayout != VK_IMAGE_LAYOUT_UNDEFINED) layoutChanges.push_back(imageBarriers.size() - 1);
}
void ImageBarrier::remove(BasicImage* image) {
    const auto it = ranges::find(images, image);
    CTH_ERR(it == std::end(images), "image not present in barrier")
        throw details->exception();
    auto index = std::distance(images.begin(), it);

    imageBarriers.erase(imageBarriers.begin() + index);
    for(auto i = 0; i <= index; ++i) if(layoutChanges[i] == index) layoutChanges.erase(layoutChanges.begin() + i);
    images.erase(it);
}
void ImageBarrier::execute(const CmdBuffer* cmd_buffer) {
    vkCmdPipelineBarrier(cmd_buffer->get(), srcStage, dstStage, 0, 0, nullptr, 0, nullptr,
        static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());
    applyChanges();
}
void ImageBarrier::applyChanges() const {
    for(auto index : layoutChanges) {
        auto& barrier = imageBarriers[index];
        auto& res = barrier.subresourceRange;

        fill_n(images[index]->_state.levelLayouts.begin() + res.baseMipLevel, res.levelCount, barrier.newLayout);
    }
}

ImageBarrier::ImageBarrier(const unordered_map<BasicImage*, ImageBarrier::Info>& images) { init(images); }
void ImageBarrier::init(const unordered_map<BasicImage*, ImageBarrier::Info>& images) { for(auto [image, info] : images) add(image, info); }

} // namespace cth

//BufferBarrier

namespace cth {

BufferBarrier::BufferBarrier(const PipelineStages stages, const unordered_map<const BasicBuffer*, Info>& buffers) : PipelineStages(stages) {
    init(buffers);
}
BufferBarrier::BufferBarrier(const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage,
    const unordered_map<const BasicBuffer*, Info>& buffers) : PipelineStages{src_stage, dst_stage} { init(buffers); }

void BufferBarrier::add(const BasicBuffer* buffer, const Info& info) {
    bufferBarriers.emplace_back(
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        nullptr,
        info.src.accessMask,
        info.dst.accessMask,
        info.src.queueFamilyIndex,
        info.dst.queueFamilyIndex,
        buffer->get(),
        0,
        buffer->size()
        );
    buffers.push_back(buffer);
}
void BufferBarrier::remove(const BasicBuffer* buffer) {
    const auto index = ranges::find(buffers, buffer);
    CTH_ERR(index == std::end(buffers), "buffer not present in barrier")
        throw details->exception();

    bufferBarriers.erase(bufferBarriers.begin() + std::distance(buffers.begin(), index));
    buffers.erase(index);
}
void BufferBarrier::execute(VkCommandBuffer cmd_buffer) {
    vkCmdPipelineBarrier(cmd_buffer, srcStage, dstStage, 0, 0, nullptr, bufferBarriers.size(), bufferBarriers.data(), 0, nullptr);
}
BufferBarrier::BufferBarrier(const unordered_map<const BasicBuffer*, Info>& buffers) { init(buffers); }
void BufferBarrier::init(const unordered_map<const BasicBuffer*, Info>& buffers) { for(auto [buffer, info] : buffers) add(buffer, info); }



} // namespace cth

//Barrier

namespace cth {

PipelineBarrier::PipelineBarrier(const PipelineStages stages, const unordered_map<const BasicBuffer*, BufferBarrier::Info>& buffers,
    const unordered_map<BasicImage*, ImageBarrier::Info>& images) : BufferBarrier(buffers), ImageBarrier(images) {
    BufferBarrier::srcStage = stages.srcStage;
    BufferBarrier::dstStage = stages.dstStage;
}
PipelineBarrier::PipelineBarrier(const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage,
    const unordered_map<const BasicBuffer*, BufferBarrier::Info>& buffers,
    const unordered_map<BasicImage*, ImageBarrier::Info>& images) : BufferBarrier(buffers), ImageBarrier(images) {
    BufferBarrier::srcStage = src_stage;
    BufferBarrier::dstStage = dst_stage;
}
void PipelineBarrier::execute(VkCommandBuffer cmd_buffer) {
    vkCmdPipelineBarrier(cmd_buffer, BufferBarrier::srcStage, BufferBarrier::dstStage, 0, 0, nullptr,
        static_cast<uint32_t>(bufferBarriers.size()), bufferBarriers.data(),
        static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());

    ImageBarrier::applyChanges();
}
void PipelineBarrier::initStages(const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage) {
    ImageBarrier::srcStage = src_stage;
    ImageBarrier::dstStage = dst_stage;
}

} // namespace cth
