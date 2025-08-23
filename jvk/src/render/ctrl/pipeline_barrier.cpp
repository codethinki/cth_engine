#include "jvk/render/ctrl/pipeline_barrier.hpp"

#include "jvk/base/core.hpp"
#include "jvk/render/cmd/cmd_buffer.hpp"
#include "jvk/res/buffer/base_buffer.hpp"
#include "jvk/res/img/image.hpp"

//ImageBarrier

namespace jvk {

ImageBarrier::ImageBarrier(Core const& core, PipelineStages stages,
    std::unordered_map<Image*, Info> const& images) : ImageBarrier{core, stages} { init(images); }

void ImageBarrier::add(Image* image, Info const& info) {
    CTH_CRITICAL(contains(image), "image already added, consider grouping") {}


    _imageBarriers.emplace_back(info.createBarrier(*image));

    bool const change = info.newLayout != constants::IMAGE_LAYOUT_IGNORED;

    if(!change) return;

    _layoutChanges.emplace_back(_imageBarriers.size() - 1, image);

}

void ImageBarrier::replace(Image* image, Info const& info) {
    auto&& rng = _imageBarriers | std::views::transform([](auto const& barrier) { return barrier.image; });
    auto const barrierIndex = static_cast<size_t>(std::distance(std::ranges::find(rng, image->get()),
        std::end(rng)));


    if(barrierIndex == _imageBarriers.size()) {
        add(image, info);
        return;
    }

    _imageBarriers[barrierIndex] = info.createBarrier(*image);


    auto images = _layoutChanges | std::views::values;
    auto const index = static_cast<uint32_t>(std::distance(std::begin(images),
        std::ranges::find(images, image)));

    if(index < _layoutChanges.size()) {
        if(info.newLayout == constants::IMAGE_LAYOUT_IGNORED)
            removeChange(barrierIndex);
    }
}



void ImageBarrier::remove(Image const* image) {
    auto const index = find(image);

    CTH_CRITICAL(static_cast<size_t>(index) == _imageBarriers.size(), "image not present in barrier") {}

    _imageBarriers.erase(std::ranges::begin(_imageBarriers) + index);

    removeChange(index);
}

void ImageBarrier::execute(CmdBuffer const& cmd_buffer) {
    core().functions()->vkCmdPipelineBarrier(cmd_buffer.get(), srcStage(), dstStage(), 0, 0, nullptr, 0,
        nullptr,
        static_cast<uint32_t>(_imageBarriers.size()), _imageBarriers.data());
    applyChanges();
}

bool ImageBarrier::contains(Image const* image) const {
    return std::ranges::any_of(_imageBarriers, [image](auto const& barrier) {
        return barrier.image == image->get();
    });
}

void ImageBarrier::applyChanges() const {
    for(auto& [index, image] : _layoutChanges) {
        auto& barrier = _imageBarriers[index];
        auto& res = barrier.subresourceRange;

        std::fill_n(image->_levelLayouts.begin() + res.baseMipLevel, res.levelCount, barrier.newLayout);
    }
}

ptrdiff_t ImageBarrier::find(Image const* image) const {
    auto&& rng = _imageBarriers | std::views::transform([](auto const& barrier) { return barrier.image; });
    return static_cast<size_t>(std::distance(std::ranges::begin(rng), std::ranges::find(rng, image->get())));
}

void ImageBarrier::removeChange(size_t barrier_index) {
    auto&& rng = _layoutChanges | std::views::keys;
    auto const it = std::ranges::find(rng, barrier_index);
    if(it == std::ranges::end(rng)) return;

    auto const changeIndex = static_cast<size_t>(std::distance(std::ranges::begin(rng), it));
    _layoutChanges.erase(std::ranges::begin(_layoutChanges) + changeIndex);
    for(size_t i = changeIndex; i < _layoutChanges.size(); ++i) _layoutChanges[i].first--;
}

void ImageBarrier::init(std::unordered_map<Image*, ImageBarrier::Info> const& images) {
    for(auto [image, info] : images) add(image, info);
}

} // namespace cth

//BufferBarrier

namespace jvk {

BufferBarrier::BufferBarrier(Core const& core, PipelineStages stages,
    std::unordered_map<BaseBuffer const*, Info> const& buffers) : BufferBarrier{
    core, stages} { init(buffers); }

void BufferBarrier::add(BaseBuffer const* buffer, Info const& info) {
    CTH_CRITICAL(std::ranges::contains(_buffers, buffer), "image already added, consider grouping") {}

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

void BufferBarrier::remove(BaseBuffer const* buffer) {
    auto const index = std::ranges::find(_buffers, buffer);
    CTH_CRITICAL(index == std::end(_buffers), "buffer not in barrier")
    throw details->exception();

    _bufferBarriers.erase(_bufferBarriers.begin() + std::distance(_buffers.begin(), index));
    _buffers.erase(index);
}

void BufferBarrier::execute(CmdBuffer const& cmd_buffer) {
    core().functions()->vkCmdPipelineBarrier(cmd_buffer.get(), srcStage(), dstStage(), 0, 0, nullptr,
        static_cast<uint32_t>(_bufferBarriers.size()),
        _bufferBarriers.data(),
        0,
        nullptr);
}

void BufferBarrier::init(std::unordered_map<BaseBuffer const*, Info> const& buffers) {
    for(auto [buffer, info] : buffers) add(buffer, info);
}



} // namespace cth

//Barrier

namespace jvk {

PipelineBarrier::PipelineBarrier(Core const& core, PipelineStages stages,
    std::unordered_map<BaseBuffer const*, BufferBarrier::Info> const& buffers,
    std::unordered_map<Image*, ImageBarrier::Info> const& images) : BarrierBase{core, stages},
    BufferBarrier{core, stages, buffers},
    ImageBarrier{core, stages, images} {}

void PipelineBarrier::execute(CmdBuffer const& cmd_buffer) {

    core().functions()->vkCmdPipelineBarrier(cmd_buffer.get(), srcStage(), dstStage(), 0, 0, nullptr,
        static_cast<uint32_t>(_bufferBarriers.size()), _bufferBarriers.data(),
        static_cast<uint32_t>(_imageBarriers.size()), _imageBarriers.data());

    applyChanges();
}

void PipelineBarrier::initStages(PipelineStages stages) {}

} // namespace cth


//ImageBarrierInfo

namespace jvk {
VkImageMemoryBarrier ImageBarrier::Info::createBarrier(Image const& image) const {
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
            static_cast<VkImageAspectFlags>(aspectMask == constants::ASPECT_MASK_IGNORED ? image.aspectMask()
                : aspectMask),
            firstMipLevel,
            levels == constants::ALL ? image.mipLevels() - firstMipLevel : levels,
            0, 1
        }};
}

} // namespace cth