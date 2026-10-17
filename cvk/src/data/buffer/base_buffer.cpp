#include "cvk/data/buffer/base_buffer.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
BaseBuffer::BaseBuffer(Core const& core, Config const& config) : _handle{core}, _memory{core, config.memory},
    _config{config} {
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
    if(state.memory.has_value()) _memory.wrap(*state.memory);
    _size = state.size;
}

void BaseBuffer::create(size_t size) {
    optDestroy();

    auto const bufferInfo = _config.createInfo(size);
    auto const allocationInfo = _memory.config().createInfo();

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
    _memory.wrap(Memory::State{vmaAllocation});
    _size = size;
}

void BaseBuffer::destroy() {
    debug_check(*this);

    _handle.reset();
    _memory.optDestroy();

    forget();
}

BaseBuffer::State BaseBuffer::release() {
    debug_check(*this);

    State state{.vkBuffer = _handle.release(), .size = _size, .memory = std::nullopt};
    if(_memory.created()) state.memory = _memory.release();

    forget();
    return state;
}

void BaseBuffer::copy(
    BaseCmdBuffer const& cmdBuffer,
    BaseBuffer const& src,
    size_t size,
    size_t srcOffset,
    size_t dstOffset
) const {
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
        "copy range must lie in the buffers"
    ) {
        details->add("src: {} + {} > {} (offset + size > src size)", srcOffset, copySize, src._size);
        details->add("dst: {} + {} > {} (offset + size > dst size)", dstOffset, copySize, _size);
    }

    VkBufferCopy2 const region{
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
        .pRegions = &region,
    };
    core().functions()->vkCmdCopyBuffer2(cmdBuffer.get(), &info);
}

void BaseBuffer::stage(BaseCmdBuffer const& cmdBuffer, BaseBuffer const& stagingBuffer, size_t dstOffset) const {
    copy(cmdBuffer, stagingBuffer, stagingBuffer.size(), 0, dstOffset);
}

void BaseBuffer::destroy(DeviceTable table, VkBuffer vkBuffer) {
    CTH_WARN(vkBuffer == VK_NULL_HANDLE, "vkBuffer invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyBuffer(table.device(), vkBuffer, nullptr);
}

void BaseBuffer::forget() { _size = 0; }
}
