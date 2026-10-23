#include "cvk/data/buffer/base_buffer.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
BaseBuffer::BaseBuffer(Core const& core, Config const& config) : _handle{core}, _config{config} {
    Core::debug_check(core);
    Config::debug_check(config);
}

BaseBuffer::BaseBuffer(Core const& core, Config const& config, State const& state) : BaseBuffer{core, config} {
    wrap(state);
}

BaseBuffer::BaseBuffer(Core const& core, Config const& config, size_t size) : BaseBuffer{core, config} { create(size); }

void BaseBuffer::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkBuffer.get(), core());
    if(state.memory.has_value()) _memory.emplace(core(), _config.memory, *state.memory);
    _size = state.size;
}

void BaseBuffer::create(size_t size) {
    optDestroy();

    auto const bufferInfo = _config.createInfo(size);
    auto const allocationInfo = _config.memory.createInfo();

    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VmaAllocation vmaAllocation = VK_NULL_HANDLE;
    auto const result = vmaCreateBuffer(
        core().allocator().get(),
        &bufferInfo,
        &allocationInfo,
        &vkBuffer,
        &vmaAllocation,
        nullptr
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create buffer") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(vkBuffer, core());
    _memory.emplace(core(), _config.memory, Memory::State{vmaAllocation});
    _size = size;
}

void BaseBuffer::destroy() {
    debug_check(*this);

    _handle.reset();
    _memory.reset();

    forget();
}

BaseBuffer::State BaseBuffer::release() {
    debug_check(*this);

    State state{.vkBuffer = _handle.release(), .size = _size, .memory = std::nullopt};
    if(_memory.has_value()) state.memory = _memory->release();

    forget();
    return state;
}

void BaseBuffer::copy(BaseCmdBuffer const& cmdBuffer, BaseBuffer const& src, BufferCopy region) const {
    auto const [srcOffset, dstOffset, size] = region;

    debug_check(*this);
    debug_check(src);
    CTH_CRITICAL(!cmdBuffer.recording(), "command buffer must be recording") {}
    CTH_CRITICAL(
        (src._config.usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0,
        "src usage must have VK_BUFFER_USAGE_TRANSFER_SRC_BIT"
    ) {}
    CTH_CRITICAL(
        (_config.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0,
        "dst usage must have VK_BUFFER_USAGE_TRANSFER_DST_BIT"
    ) {}
    CTH_CRITICAL(srcOffset > src._size || dstOffset > _size, "offsets must lie in the buffers") {}

    size_t const copySize = size == VK_WHOLE_SIZE ? std::min(src._size - srcOffset, _size - dstOffset) : size;

    CTH_CRITICAL(copySize == 0, "copy size must not be 0") {}
    CTH_CRITICAL(
        copySize > src._size - srcOffset || copySize > _size - dstOffset,
        "copy range must lie in the buffers, got src {} + {} of {}, dst {} + {} of {}",
        srcOffset,
        copySize,
        src._size,
        dstOffset,
        copySize,
        _size
    ) {}

    VkBufferCopy2 const vkRegion{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .pNext = nullptr,
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = copySize,
    };
    VkCopyBufferInfo2 const info{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .pNext = nullptr,
        .srcBuffer = src.get(),
        .dstBuffer = get(),
        .regionCount = 1,
        .pRegions = &vkRegion,
    };
    core().functions()->vkCmdCopyBuffer2(cmdBuffer.get(), &info);
}

void BaseBuffer::copy(
    BaseCmdBuffer const& cmdBuffer,
    Image const& src,
    size_t dstOffset,
    uint32_t mipLevel,
    VkImageLayout layout
) const {
    src.copyTo(cmdBuffer, *this, dstOffset, mipLevel, layout);
}

Mapping<std::byte> BaseBuffer::map(Range range) const {
    debug_check(*this);
    return ownMemory().map(range);
}

void BaseBuffer::flush(Range range) const {
    debug_check(*this);
    ownMemory().flush(range);
}

void BaseBuffer::invalidate(Range range) const {
    debug_check(*this);
    ownMemory().invalidate(range);
}

void BaseBuffer::write(std::span<std::byte const> data, size_t offset) const {
    debug_check(*this);
    ownMemory().write(data, offset);
}

void BaseBuffer::destroy(DeviceTable table, VkBuffer vkBuffer) {
    CTH_WARN(vkBuffer == VK_NULL_HANDLE, "vkBuffer invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyBuffer(table.device(), vkBuffer, nullptr);
}

void BaseBuffer::forget() {
    _memory.reset();
    _size = 0;
}
}
