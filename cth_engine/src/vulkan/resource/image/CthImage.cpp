#include "CthImage.hpp"

#include "../buffer/CthBaseBuffer.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthPipelineBarrier.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


namespace cth::vk {
using namespace std;


Image::Image(cth::not_null<Core const*> core, Config const& config) : _core{core}, _config{config},
    _memory{make_unique<Memory>(_core, _config.memoryProperties)} {
    DEBUG_CHECK_CORE(core);
    _levelLayouts.resize(_config.mipLevels);
    std::ranges::fill(_levelLayouts, _config.initialLayout);
}
Image::Image(cth::not_null<Core const*> core, Config const& config, VkExtent2D extent) : Image{core, config} { create(extent); }
Image::Image(cth::not_null<Core const*> core, Config const& config, State state) : Image{core, config} { wrap(std::move(state)); }


Image::~Image() { optDestroy(); }

void Image::wrap(State state) {
    DEBUG_CHECK_IMAGE_HANDLE(state.vkImage);

    optDestroy();


    _handle = state.vkImage.get();

    _levelLayouts = state.levelLayouts;
    while(_levelLayouts.size() != _config.mipLevels) _levelLayouts.push_back(_config.initialLayout);

    if(state.bound && state.memory == nullptr) return;

    if(state.memory != nullptr) _memory = std::move(state.memory);
    else alloc();
    if(!state.bound) bind();


}

void Image::create(VkExtent2D extent) {
    optDestroy();

    _extent = extent;

    createHandle();

    alloc();
    bind();
}



void Image::destroy() {
    DEBUG_CHECK_IMAGE(this);

    auto const queue = _core->destructionQueue();

    if(_memory->created()) _memory->destroy();

    if(queue) queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    reset();
}
Image::State Image::release() {
    DEBUG_CHECK_IMAGE(this);

    State state{
        _extent,
        _handle.get(),
        true,
        std::move(_memory),
        std::move(_levelLayouts),
    };

    reset();

    return state;
}

Image::TransitionConfig Image::TransitionConfig::Create(VkImageLayout current_layout, VkImageLayout new_layout) {
    if(current_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        return {0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
    if(current_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    if(current_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
    if(current_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return {VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    CTH_ERR(true, "unsupported layout transition") {
        details->add("transition: {0} -> {1}", static_cast<uint32_t>(current_layout), static_cast<uint32_t>(new_layout));
        throw details->exception();
    }

    return {};
}



void Image::copy(CmdBuffer const& cmd_buffer, BaseBuffer const& src_buffer, size_t src_offset, uint32_t mip_level) const {
    DEBUG_CHECK_IMAGE(this);

    CTH_WARN(_levelLayouts[mip_level] != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        "PERFORMANCE: image layout is not transfer dst optional") {}


    VkBufferImageCopy region;
    region.bufferOffset = src_offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    //const auto mask = aspect_mask == VK_IMAGE_ASPECT_NONE ? _config.aspectFlags : aspect_mask;
    region.imageSubresource.aspectMask = _config.aspectMask;

    region.imageSubresource.mipLevel = mip_level;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {_extent.width, _extent.height, 1};
    vkCmdCopyBufferToImage(cmd_buffer.get(), src_buffer.get(), _handle.get(), _levelLayouts[mip_level], 1, &region);
}

void Image::transitionLayout(CmdBuffer const& cmd_buffer, VkImageLayout new_layout, uint32_t first_mip_level,
    uint32_t mip_levels) {
    auto [srcAccess, dstAccess, srcStage, dstStage] = TransitionConfig::Create(_levelLayouts[first_mip_level], new_layout);

    ImageBarrier barrier{srcStage, dstStage};

    transitionLayout(barrier, new_layout, srcAccess, dstAccess, first_mip_level, mip_levels);

    barrier.execute(cmd_buffer);
}
void Image::transitionLayout(ImageBarrier& barrier, VkImageLayout new_layout, VkAccessFlags src_access,
    VkAccessFlags dst_access, uint32_t first_mip_level, uint32_t mip_levels) {
    DEBUG_CHECK_IMAGE(this);

    auto const oldLayout = _levelLayouts[first_mip_level];
    CTH_ERR(
        any_of(_levelLayouts.begin() + first_mip_level, mip_levels == constants::ALL ? _levelLayouts.end() : _levelLayouts.begin() + first_mip_level +
            mip_levels,
            [oldLayout](VkImageLayout layout) { return oldLayout != layout; }), "all transitioned layouts must be the same")
        throw details->exception();

    barrier.add(this, ImageBarrier::Info::LayoutTransition(new_layout, src_access, dst_access, first_mip_level, mip_levels));
}
uint32_t Image::evalMipLevelCount(VkExtent2D extent) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height))) + 1);
}


void Image::destroy(VkDevice vk_device, VkImage vk_image) {
    CTH_WARN(vk_image == VK_NULL_HANDLE, "vk_image should not be invalid (VK_NULL_HANDLE)") {}
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);

    vkDestroyImage(vk_device, vk_image, nullptr);
}

void Image::createHandle() {
    auto createInfo = _config.createInfo();

    createInfo.extent.width = _extent.width;
    createInfo.extent.height = _extent.height;

    VkImage ptr = VK_NULL_HANDLE;

    auto const createResult = vkCreateImage(_core->vkDevice(), &createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create image")
        throw vk::result_exception{createResult, details->exception()};

    _handle = ptr;
}

void Image::alloc() const {
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_core->vkDevice(), _handle.get(), &memRequirements);

    _memory->create(memRequirements);
}
void Image::bind() const {
    auto const bindResult = vkBindImageMemory(_core->vkDevice(), _handle.get(), _memory->get(), 0);

    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "failed to bind image memory")
        throw vk::result_exception{bindResult, details->exception()};

}
void Image::reset() {
    DEBUG_CHECK_IMAGE(this);

    _handle = VK_NULL_HANDLE;
    _extent = {0, 0};

    _levelLayouts.resize(_config.mipLevels);
    std::ranges::fill_n(_levelLayouts.begin(), _config.mipLevels, _config.initialLayout);
}



#ifdef CONSTANT_DEBUG_MODE
void Image::debug_check(cth::not_null<Image const*> image) {
    CTH_ERR(!image->created(), "image must be created") throw details->exception();
    DEBUG_CHECK_IMAGE_HANDLE(image->get());
}
void Image::debug_check_handle([[maybe_unused]] vk::not_null<VkImage> vk_image) {}

#endif

} // namespace cth

//Config & State

namespace cth::vk {
VkImageCreateInfo Image::Config::createInfo() const {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.arrayLayers = 1;
    info.extent.depth = 1;

    info.initialLayout = initialLayout;
    info.format = format;
    info.tiling = tiling;
    info.usage = usage;
    info.mipLevels = mipLevels;
    info.samples = samples;

    return info;
}
}
