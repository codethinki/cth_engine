#include "CthBasicImage.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthPipelineBarrier.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/resource/buffer/CthBasicBuffer.hpp"
#include "vulkan/resource/memory/CthBasicMemory.hpp"
#include "vulkan/utility/CthVkUtils.hpp"



namespace cth::vk {

BasicImage::BasicImage(const BasicCore* core, const VkExtent2D extent, const Config& config) : _core(core),
    _extent(extent), _config(config) { init(); }

BasicImage::BasicImage(const BasicCore* core, const VkExtent2D extent, const Config& config, VkImage vk_image, State state) : _core(core),
    _extent(extent), _config(config), _state(std::move(state)), _handle(vk_image) { init(); }

void BasicImage::wrap(VkImage vk_image, const State& state) {
    DEBUG_CHECK_IMAGE(this);
    DEBUG_CHECK_MEMORY_LEAK(_state.memory.get());

    _handle = vk_image;
    _state = state;

    init();
}

void BasicImage::create() {
    CTH_INFORM(_handle != VK_NULL_HANDLE, "image replaced") {}

    auto createInfo = _config.createInfo();

    createInfo.extent.width = _extent.width;
    createInfo.extent.height = _extent.height;

    VkImage ptr = VK_NULL_HANDLE;

    const auto createResult = vkCreateImage(_core->vkDevice(), &createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create image")
        throw except::vk_result_exception{createResult, details->exception()};

    _handle = ptr;
}

void BasicImage::alloc(BasicMemory* new_memory) {
    CTH_INFORM(_state.memory != nullptr, "memory replaced") {}

    setMemory(new_memory);
    alloc();
}
void BasicImage::alloc() const {
    DEBUG_CHECK_IMAGE_NOT_BOUND(this);
    DEBUG_CHECK_IMAGE(this);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_core->vkDevice(), _handle.get(), &memRequirements);

    _state.memory->alloc(memRequirements);
}

void BasicImage::bind(BasicMemory* new_memory) {
    setMemory(new_memory);
    bind();
}
void BasicImage::bind() {
    DEBUG_CHECK_IMAGE_NOT_BOUND(this);
    DEBUG_CHECK_IMAGE(this);
    DEBUG_CHECK_MEMORY(_state.memory.get());

    const auto bindResult = vkBindImageMemory(_core->vkDevice(), _handle.get(), _state.memory->get(), 0);

    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "failed to bind image memory")
        throw except::vk_result_exception{bindResult, details->exception()};

    _state.bound = true;
}

void BasicImage::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) {
        DEBUG_CHECK_DELETION_QUEUE(deletion_queue);
        deletion_queue->push(_handle.get());
    } else destroy(_core->vkDevice(), _handle.get());

    _handle = VK_NULL_HANDLE;

    reset();
}



void BasicImage::copy(const CmdBuffer& cmd_buffer, const BasicBuffer& src_buffer, const size_t src_offset, const uint32_t mip_level) const {
    DEBUG_CHECK_IMAGE(this);

    CTH_WARN(_state.levelLayouts[mip_level] != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
    vkCmdCopyBufferToImage(cmd_buffer.get(), src_buffer.get(), _handle.get(), _state.levelLayouts[mip_level], 1, &region);
}

void BasicImage::transitionLayout(const CmdBuffer& cmd_buffer, const VkImageLayout new_layout, const uint32_t first_mip_level,
    const uint32_t mip_levels) {


    auto [srcAccess, dstAccess, srcStage, dstStage] = transitionConfig(_state.levelLayouts[first_mip_level], new_layout);

    ImageBarrier barrier{srcStage, dstStage};

    transitionLayout(barrier, new_layout, srcAccess, dstAccess, first_mip_level, mip_levels);

    barrier.execute(cmd_buffer);
}
void BasicImage::transitionLayout(ImageBarrier& barrier, const VkImageLayout new_layout, const VkAccessFlags src_access,
    const VkAccessFlags dst_access, const uint32_t first_mip_level, const uint32_t mip_levels) {
    DEBUG_CHECK_IMAGE(this);

    const auto oldLayout = _state.levelLayouts[first_mip_level];
    CTH_ERR(any_of(_state.levelLayouts.begin() + first_mip_level,
        mip_levels == constant::ALL ? _state.levelLayouts.end() : _state.levelLayouts.begin() + first_mip_level + mip_levels,
        [oldLayout](VkImageLayout layout) { return oldLayout != layout; }), "all transitioned layouts must be the same")
        throw details->exception();

    barrier.add(this, ImageBarrier::Info::LayoutTransition(new_layout, src_access, dst_access, first_mip_level, mip_levels));
}


void BasicImage::destroy(VkDevice vk_device, VkImage vk_image) {
    CTH_WARN(vk_image == VK_NULL_HANDLE, "vk_image invalid") {}
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);

    vkDestroyImage(vk_device, vk_image, nullptr);
}

uint32_t BasicImage::evalMipLevelCount(const VkExtent2D extent) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height))) + 1);
}

void BasicImage::reset() {
    DEBUG_CHECK_IMAGE_LEAK(this);

    _handle = VK_NULL_HANDLE;
    _state.reset(_config);
}
BasicMemory* BasicImage::releaseMemory() {
    BasicMemory* mem = _state.memory.get();
    _state.memory = nullptr;
    return mem;
}

void BasicImage::setMemory(BasicMemory* new_memory) {
    CTH_ERR(new_memory == _state.memory.get(), "new_memory must not be current memory") throw details->exception();
    CTH_ERR(new_memory == nullptr, "new_memory must not be nullptr") throw details->exception();
    DEBUG_CHECK_IMAGE_NOT_BOUND(this);

    _state.memory = new_memory;
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
    DEBUG_CHECK_CORE(_core);
    _state.init(_config);
}



#ifdef CONSTANT_DEBUG_MODE
void BasicImage::debug_check(const BasicImage* image) {
    CTH_ERR(image == nullptr, "image must not be nullptr") throw details->exception();
    CTH_ERR(image->_handle == VK_NULL_HANDLE, "image must be a valid handle") throw details->exception();
}
void BasicImage::debug_check_leak(const BasicImage* image) {
    CTH_WARN(image->_handle != VK_NULL_HANDLE, "image handle replaced (potential memory leak)") {}
}
void BasicImage::debug_check_memory_leak(const BasicImage* image) {
    CTH_WARN(image->_state.memory != nullptr, "memory ptr replaced (potential memory leak)") {}
}
void BasicImage::debug_not_bound_check(const BasicImage* image) {
    CTH_ERR(image->_state.bound, "image must not be bound") throw details->exception();
}
#endif

} // namespace cth

//Config & State

namespace cth::vk {
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
