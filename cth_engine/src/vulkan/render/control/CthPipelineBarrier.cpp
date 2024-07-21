#include "CthPipelineBarrier.hpp"

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/resource/buffer/CthBasicBuffer.hpp"
#include "vulkan/resource/image/CthBasicImage.hpp"

//ImageBarrier

namespace cth::vk {

ImageBarrier::ImageBarrier(PipelineStages const stages, std::unordered_map<BasicImage*, ImageBarrier::Info> const& images) : PipelineStages(stages) {
    init(images);
}
ImageBarrier::ImageBarrier(VkPipelineStageFlags const src_stage, VkPipelineStageFlags const dst_stage,
    std::unordered_map<BasicImage*, ImageBarrier::Info> const& images) : PipelineStages{src_stage, dst_stage} { init(images); }

void ImageBarrier::add(BasicImage* image, Info const& info) {
    CTH_ERR(contains(image), "image already added, consider grouping") throw details->exception();


    _imageBarriers.emplace_back(info.createBarrier(*image));

    bool const change = info.newLayout != constants::IMAGE_LAYOUT_IGNORED;

    if(!change) return;

    _layoutChanges.emplace_back(_imageBarriers.size() - 1, image);

}
void ImageBarrier::replace(BasicImage* image, Info const& info) {
    auto&& rng = _imageBarriers | std::views::transform([](auto const& barrier) { return barrier.image; });
    auto const barrierIndex = static_cast<size_t>(std::distance(std::ranges::find(rng, image->get()), std::end(rng)));


    if(barrierIndex == _imageBarriers.size()) {
        add(image, info);
        return;
    }

    _imageBarriers[barrierIndex] = info.createBarrier(*image);


    auto images = _layoutChanges | std::views::values;
    auto const index = static_cast<uint32_t>(std::distance(std::begin(images), std::ranges::find(images, image)));

    if(index < _layoutChanges.size()) {
        if(info.newLayout == constants::IMAGE_LAYOUT_IGNORED)
            removeChange(barrierIndex);
    }
}



void ImageBarrier::remove(BasicImage const* image) {
    auto const index = find(image);

    CTH_ERR(static_cast<size_t>(index) == _imageBarriers.size(), "image not present in barrier") throw details->exception();

    _imageBarriers.erase(std::ranges::begin(_imageBarriers) + index);

    removeChange(index);
}
void ImageBarrier::execute(CmdBuffer const& cmd_buffer) {
    vkCmdPipelineBarrier(cmd_buffer.get(), srcStage, dstStage, 0, 0, nullptr, 0, nullptr,
        static_cast<uint32_t>(_imageBarriers.size()), _imageBarriers.data());
    applyChanges();
}
bool ImageBarrier::contains(BasicImage const* image) const {
    return std::ranges::any_of(_imageBarriers, [image](auto const& barrier) { return barrier.image == image->get(); });
}

void ImageBarrier::applyChanges() const {
    for(auto& [index, image] : _layoutChanges) {
        auto& barrier = _imageBarriers[index];
        auto& res = barrier.subresourceRange;

        std::fill_n(image->_state.levelLayouts.begin() + res.baseMipLevel, res.levelCount, barrier.newLayout);
    }
}

ImageBarrier::ImageBarrier(std::unordered_map<BasicImage*, ImageBarrier::Info> const& images) { init(images); }
ptrdiff_t ImageBarrier::find(BasicImage const* image) const {
    auto&& rng = _imageBarriers | std::views::transform([](auto const& barrier) { return barrier.image; });
    return static_cast<size_t>(std::distance(std::ranges::begin(rng), std::ranges::find(rng, image->get())));
}
void ImageBarrier::removeChange(size_t const barrier_index) {
    auto&& rng = _layoutChanges | std::views::keys;
    auto const it = std::ranges::find(rng, barrier_index);
    if(it == std::ranges::end(rng)) return;

    auto const changeIndex = static_cast<size_t>(std::distance(std::ranges::begin(rng), it));
    _layoutChanges.erase(std::ranges::begin(_layoutChanges) + changeIndex);
    for(size_t i = changeIndex; i < _layoutChanges.size(); ++i) _layoutChanges[i].first--;
}
void ImageBarrier::init(std::unordered_map<BasicImage*, ImageBarrier::Info> const& images) { for(auto [image, info] : images) add(image, info); }

} // namespace cth

//BufferBarrier

namespace cth::vk {

BufferBarrier::BufferBarrier(PipelineStages const stages, std::unordered_map<BasicBuffer const*, Info> const& buffers) : PipelineStages(stages) {
    init(buffers);
}
BufferBarrier::BufferBarrier(VkPipelineStageFlags const src_stage, VkPipelineStageFlags const dst_stage,
    std::unordered_map<BasicBuffer const*, Info> const& buffers) : PipelineStages{src_stage, dst_stage} { init(buffers); }

void BufferBarrier::add(BasicBuffer const* buffer, Info const& info) {
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
void BufferBarrier::remove(BasicBuffer const* buffer) {
    auto const index = std::ranges::find(_buffers, buffer);
    CTH_ERR(index == std::end(_buffers), "buffer not present in barrier")
        throw details->exception();

    _bufferBarriers.erase(_bufferBarriers.begin() + std::distance(_buffers.begin(), index));
    _buffers.erase(index);
}
void BufferBarrier::execute(CmdBuffer const& cmd_buffer) {
    vkCmdPipelineBarrier(cmd_buffer.get(), srcStage, dstStage, 0, 0, nullptr, static_cast<uint32_t>(_bufferBarriers.size()), _bufferBarriers.data(), 0,
        nullptr);
}
BufferBarrier::BufferBarrier(std::unordered_map<BasicBuffer const*, Info> const& buffers) { init(buffers); }
void BufferBarrier::init(std::unordered_map<BasicBuffer const*, Info> const& buffers) { for(auto [buffer, info] : buffers) add(buffer, info); }



} // namespace cth

//Barrier

namespace cth::vk {

PipelineBarrier::PipelineBarrier(PipelineStages const stages, std::unordered_map<BasicBuffer const*, BufferBarrier::Info> const& buffers,
    std::unordered_map<BasicImage*, ImageBarrier::Info> const& images) : BufferBarrier(buffers), ImageBarrier(images) {
    BufferBarrier::srcStage = stages.srcStage;
    BufferBarrier::dstStage = stages.dstStage;
}
PipelineBarrier::PipelineBarrier(VkPipelineStageFlags const src_stage, VkPipelineStageFlags const dst_stage,
    std::unordered_map<BasicBuffer const*, BufferBarrier::Info> const& buffers,
    std::unordered_map<BasicImage*, ImageBarrier::Info> const& images) : BufferBarrier(buffers), ImageBarrier(images) {
    BufferBarrier::srcStage = src_stage;
    BufferBarrier::dstStage = dst_stage;
}
void PipelineBarrier::execute(CmdBuffer const& cmd_buffer) {
    vkCmdPipelineBarrier(cmd_buffer.get(), BufferBarrier::srcStage, BufferBarrier::dstStage, 0, 0, nullptr,
        static_cast<uint32_t>(_bufferBarriers.size()), _bufferBarriers.data(),
        static_cast<uint32_t>(_imageBarriers.size()), _imageBarriers.data());

    ImageBarrier::applyChanges();
}
void PipelineBarrier::initStages(VkPipelineStageFlags const src_stage, VkPipelineStageFlags const dst_stage) {
    ImageBarrier::srcStage = src_stage;
    ImageBarrier::dstStage = dst_stage;
}

} // namespace cth


//ImageBarrierInfo

namespace cth::vk {
VkImageMemoryBarrier ImageBarrier::Info::createBarrier(BasicImage const& image) const {
    return VkImageMemoryBarrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        nullptr,
        src.accessMask,
        dst.accessMask,
        image.layout(firstMipLevel),
        newLayout == constants::IMAGE_LAYOUT_IGNORED ? image.layout(firstMipLevel) : newLayout,
        src.queueFamilyIndex,
        dst.queueFamilyIndex,
        image.get(),
        VkImageSubresourceRange{
            static_cast<VkImageAspectFlags>(aspectMask == constants::ASPECT_MASK_IGNORED ? image.aspectMask() : aspectMask),
            firstMipLevel,
            levels == constants::ALL ? image.mipLevels() - firstMipLevel : levels,
            0, 1
        }};
}

} // namespace cth
