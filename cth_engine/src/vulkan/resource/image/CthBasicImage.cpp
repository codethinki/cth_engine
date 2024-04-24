#include "CthBasicImage.hpp"

#include <vulkan/resource/CthDeletionQueue.hpp>
#include <vulkan/resource/buffer/CthBasicBuffer.hpp>
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/pipeline/CthPipelineBarrier.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/resource/CthMemory.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>



namespace cth {

BasicImage::BasicImage(Device* device, const VkExtent2D extent, const Config& config) : device(device),
    _extent(extent), _config(config) { init(); }

BasicImage::BasicImage(Device* device, const VkExtent2D extent, const Config& config, VkImage vk_image, const State& state) : device(device),
    _extent(extent), _config(config), vkImage(vk_image), _state(state) { init(); }

void BasicImage::create() {
    auto createInfo = _config.createInfo();

    createInfo.extent.width = _extent.width;
    createInfo.extent.height = _extent.height;

    const auto createResult = vkCreateImage(device->get(), &createInfo, nullptr, &vkImage);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create image")
        throw except::vk_result_exception{createResult, details->exception()};
}

void BasicImage::alloc(BasicMemory* new_memory) {
    setMemory(new_memory);
    alloc();
}
void BasicImage::alloc() const {
    debug_not_bound_check(this);
    debug_check(this);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device->get(), vkImage, &memRequirements);

    _state.memory->alloc(memRequirements);
}

void BasicImage::bind(BasicMemory* new_memory) {
    setMemory(new_memory);
    bind();
}
void BasicImage::bind() {
    debug_not_bound_check(this);
    debug_check(this);
    BasicMemory::debug_check(_state.memory);

    const auto bindResult = vkBindImageMemory(device->get(), vkImage, _state.memory->get(), 0);

    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "failed to bind image memory")
        throw except::vk_result_exception{bindResult, details->exception()};

    _state.bound = true;
}

void BasicImage::destroy(DeletionQueue* deletion_queue) {
    if(!deletion_queue) destroy(device, vkImage);
    else deletion_queue->push(vkImage);
    reset();
}
void BasicImage::setMemory(BasicMemory* new_memory) {
    CTH_ERR(new_memory == _state.memory, "new_memory must not be current memory") throw details->exception();
    CTH_ERR(new_memory == nullptr, "new_memory must not be nullptr") throw details->exception();
    debug_not_bound_check(this);
    _state.memory = new_memory;
}


void BasicImage::copy(const CmdBuffer* cmd_buffer, const BasicBuffer* src_buffer, const size_t src_offset, const uint32_t mip_level) const {
    debug_check(this);

    CTH_ERR(src_buffer == nullptr, "buffer must not be nullptr") throw details->exception();
    CTH_WARN(_state.levelLayouts[mip_level] != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        "PERFORMANCE: image layout is not transfer dst optional");


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
    vkCmdCopyBufferToImage(cmd_buffer->get(), src_buffer->get(), vkImage, _state.levelLayouts[mip_level], 1, &region);
}

void BasicImage::transitionLayout(const CmdBuffer* cmd_buffer, const VkImageLayout new_layout, const uint32_t first_mip_level,
    const uint32_t mip_levels) {


    auto [srcAccess, dstAccess, srcStage, dstStage] = transitionConfig(_state.levelLayouts[first_mip_level], new_layout);

    ImageBarrier barrier{srcStage, dstStage};

    transitionLayout(&barrier, srcAccess, dstAccess, new_layout, first_mip_level, mip_levels);

    barrier.execute(cmd_buffer);
}
void BasicImage::transitionLayout(ImageBarrier* barrier, const VkAccessFlags src_access, const VkAccessFlags dst_access,
    const VkImageLayout new_layout, const uint32_t first_mip_level, const uint32_t mip_levels) {
    debug_check(this);

    const auto levels = mip_levels == 0 ? _config.mipLevels - first_mip_level : mip_levels;

    const auto oldLayout = _state.levelLayouts[first_mip_level];
    CTH_ERR(any_of(_state.levelLayouts.begin() + first_mip_level, _state.levelLayouts.begin() + first_mip_level + levels,
        [oldLayout](VkImageLayout layout) { return oldLayout != layout; }), "all transitioned layouts must be the same")
        throw details->exception();

    barrier->add(this, ImageBarrier::Info::LayoutTransition(src_access, dst_access, new_layout, first_mip_level, levels));
}


void BasicImage::destroy(const Device* device, VkImage image) {
    CTH_WARN(image == VK_NULL_HANDLE, "vk_image invalid");
    Device::debug_check(device);

    vkDestroyImage(device->get(), image, nullptr);
}

uint32_t BasicImage::evalMipLevelCount(const VkExtent2D extent) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height))) + 1);
}

void BasicImage::reset() {
    _state.reset(_config);
    vkImage = VK_NULL_HANDLE;
}

BasicImage::transition_config BasicImage::transitionConfig(const VkImageLayout current_layout, const VkImageLayout new_layout) {
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



void BasicImage::init() {
    Device::debug_check(device);
    _state.init(_config);
}



#ifdef _DEBUG
void BasicImage::debug_check(const BasicImage* image) {
    CTH_ERR(image == nullptr, "image must not be nullptr") throw details->exception();
    CTH_ERR(image->vkImage == VK_NULL_HANDLE, "image must be a valid handle") throw details->exception();
}
void BasicImage::debug_not_bound_check(const BasicImage* image) {
    CTH_ERR(image->_state.bound, "image must not be bound") throw details->exception();
}
#endif

} // namespace cth

//Config & State

namespace cth {
VkImageCreateInfo BasicImage::Config::createInfo() const {
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

void BasicImage::State::reset(const Config& config) {
    levelLayouts.resize(config.mipLevels);
    std::ranges::fill_n(levelLayouts.begin(), config.mipLevels, config.initialLayout);

    bound = false;
}
void BasicImage::State::init(const Config& config) { while(levelLayouts.size() != config.mipLevels) levelLayouts.push_back(config.initialLayout); }


} // namespace cth
