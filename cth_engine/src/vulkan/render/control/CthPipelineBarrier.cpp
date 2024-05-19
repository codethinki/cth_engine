#include "CthPipelineBarrier.hpp"

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/resource/buffer/CthBasicBuffer.hpp"
#include "vulkan/resource/image/CthBasicImage.hpp"

#include <ranges>
//ImageBarrier

namespace cth {

ImageBarrier::ImageBarrier(const PipelineStages stages, const unordered_map<BasicImage*, ImageBarrier::Info>& images) : PipelineStages(stages) {
    init(images);
}
ImageBarrier::ImageBarrier(const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage,
    const unordered_map<BasicImage*, ImageBarrier::Info>& images) : PipelineStages{src_stage, dst_stage} { init(images); }

void ImageBarrier::add(BasicImage* image, const Info& info) {
    CTH_ERR(contains(image), "image already added, consider grouping") throw details->exception();


    _imageBarriers.emplace_back(info.createBarrier(*image));

    const bool change = info.newLayout != Constant::IMAGE_LAYOUT_IGNORED;

    if(!change) return;

    _layoutChanges.emplace_back(_imageBarriers.size() - 1, image);

}
void ImageBarrier::replace(BasicImage* image, const Info& info) {
    auto&& rng = _imageBarriers | views::transform([](const auto& barrier) { return barrier.image; });
    const auto barrierIndex = static_cast<size_t>(distance(ranges::find(rng, image->get()), std::end(rng)));


    if(barrierIndex == _imageBarriers.size()) {
        add(image, info);
        return;
    }

    _imageBarriers[barrierIndex] = info.createBarrier(*image);


    auto images = _layoutChanges | views::values;
    auto index = static_cast<uint32_t>(std::distance(std::begin(images), ranges::find(images, image)));

    if(index < _layoutChanges.size()) {
        if(info.newLayout == Constant::IMAGE_LAYOUT_IGNORED)
            removeChange(barrierIndex);
    }
}



void ImageBarrier::remove(const BasicImage* image) {
    const auto index = find(image);

    CTH_ERR(static_cast<size_t>(index) == _imageBarriers.size(), "image not present in barrier") throw details->exception();

    _imageBarriers.erase(ranges::begin(_imageBarriers) + index);

    removeChange(index);
}
void ImageBarrier::execute(const CmdBuffer& cmd_buffer) {
    vkCmdPipelineBarrier(cmd_buffer.get(), srcStage, dstStage, 0, 0, nullptr, 0, nullptr,
        static_cast<uint32_t>(_imageBarriers.size()), _imageBarriers.data());
    applyChanges();
}
bool ImageBarrier::contains(const BasicImage* image) const {
    return std::ranges::any_of(_imageBarriers, [image](const auto& barrier) { return barrier.image == image->get(); });
}

void ImageBarrier::applyChanges() const {
    for(auto& [index, image] : _layoutChanges) {
        auto& barrier = _imageBarriers[index];
        auto& res = barrier.subresourceRange;

        fill_n(image->_state.levelLayouts.begin() + res.baseMipLevel, res.levelCount, barrier.newLayout);
    }
}

ImageBarrier::ImageBarrier(const unordered_map<BasicImage*, ImageBarrier::Info>& images) { init(images); }
ptrdiff_t ImageBarrier::find(const BasicImage* image) const {
    auto&& rng = _imageBarriers | views::transform([](const auto& barrier) { return barrier.image; });
    return static_cast<size_t>(distance(ranges::begin(rng), ranges::find(rng, image->get())));
}
void ImageBarrier::removeChange(const size_t barrier_index) {
    auto&& rng = _layoutChanges | views::keys;
    auto it = ranges::find(rng, barrier_index);
    if(it == ranges::end(rng)) return;

    auto changeIndex = static_cast<size_t>(std::distance(ranges::begin(rng), it));
    _layoutChanges.erase(ranges::begin(_layoutChanges) + changeIndex);
    for(size_t i = changeIndex; i < _layoutChanges.size(); ++i) _layoutChanges[i].first--;
}
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
    CTH_ERR(std::ranges::contains(_buffers, buffer), "image already added, consider grouping") throw details->exception();

    _bufferBarriers.emplace_back(
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
    _buffers.push_back(buffer);
}
void BufferBarrier::remove(const BasicBuffer* buffer) {
    const auto index = ranges::find(_buffers, buffer);
    CTH_ERR(index == std::end(_buffers), "buffer not present in barrier")
        throw details->exception();

    _bufferBarriers.erase(_bufferBarriers.begin() + std::distance(_buffers.begin(), index));
    _buffers.erase(index);
}
void BufferBarrier::execute(const CmdBuffer& cmd_buffer) {
    vkCmdPipelineBarrier(cmd_buffer.get(), srcStage, dstStage, 0, 0, nullptr, static_cast<uint32_t>(_bufferBarriers.size()), _bufferBarriers.data(), 0,
        nullptr);
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
void PipelineBarrier::execute(const CmdBuffer& cmd_buffer) {
    vkCmdPipelineBarrier(cmd_buffer.get(), BufferBarrier::srcStage, BufferBarrier::dstStage, 0, 0, nullptr,
        static_cast<uint32_t>(_bufferBarriers.size()), _bufferBarriers.data(),
        static_cast<uint32_t>(_imageBarriers.size()), _imageBarriers.data());

    ImageBarrier::applyChanges();
}
void PipelineBarrier::initStages(const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage) {
    ImageBarrier::srcStage = src_stage;
    ImageBarrier::dstStage = dst_stage;
}

} // namespace cth


//ImageBarrierInfo

namespace cth {
VkImageMemoryBarrier ImageBarrier::Info::createBarrier(const BasicImage& image) const {
    return VkImageMemoryBarrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        nullptr,
        src.accessMask,
        dst.accessMask,
        image.layout(firstMipLevel),
        newLayout == Constant::IMAGE_LAYOUT_IGNORED ? image.layout(firstMipLevel) : newLayout,
        src.queueFamilyIndex,
        dst.queueFamilyIndex,
        image.get(),
        VkImageSubresourceRange{
            static_cast<VkImageAspectFlags>(aspectMask == Constant::ASPECT_MASK_IGNORED ? image.aspectMask() : aspectMask),
            firstMipLevel,
            levels == Constant::ALL ? image.mipLevels() - firstMipLevel : levels,
            0, 1
        }};
}

} // namespace cth
