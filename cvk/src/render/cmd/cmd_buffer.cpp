#include "cvk/render/cmd/cmd_buffer.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <ranges>


namespace cvk {
using std::span;

BaseCmdBuffer::BaseCmdBuffer(CmdPool const& pool, VkCommandBufferLevel level) : _handle{pool}, _level{level} {
    CmdPool::debug_check(pool);
}

void BaseCmdBuffer::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkBuffer.get(), pool());
    ++pool()._buffers;
}

void BaseCmdBuffer::create() {
    CmdPool::debug_check(pool());

    optDestroy();

    VkCommandBufferAllocateInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = pool().get(),
        .level = _level,
        .commandBufferCount = 1,
    };

    VkCommandBuffer ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkAllocateCommandBuffers(core().vkDevice(), &info, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to allocate command buffer") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, pool());
    ++pool()._buffers;
}

void BaseCmdBuffer::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

BaseCmdBuffer::State BaseCmdBuffer::release() {
    debug_check(*this);

    State const state{_handle.release()};
    --pool()._buffers;

    forget();
    return state;
}

void BaseCmdBuffer::end() {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}

    auto const result = core().functions()->vkEndCommandBuffer(get());
    _recording = false;

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to end command buffer") {}
}

void BaseCmdBuffer::reset(VkCommandBufferResetFlags flags) {
    debug_check(*this);
    CTH_CRITICAL(
        (pool().config().flags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) == 0,
        "pool must allow command buffer resets"
    ) {}

    auto const result = core().functions()->vkResetCommandBuffer(get(), flags);
    _recording = false;

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to reset command buffer") {}
}

void BaseCmdBuffer::destroy(DeviceTable table, VkCommandPool vkPool, VkCommandBuffer vkBuffer) {
    CTH_WARN(vkBuffer == VK_NULL_HANDLE, "vkBuffer invalid (VK_NULL_HANDLE)") {}

    table->vkFreeCommandBuffers(table.device(), vkPool, 1, &vkBuffer);
}

void BaseCmdBuffer::destroy(DeviceTable table, VkCommandPool vkPool, span<VkCommandBuffer const> vkBuffers) {
    CTH_WARN(std::ranges::contains(vkBuffers, VK_NULL_HANDLE), "> 0 vkBuffers invalid (VK_NULL_HANDLE)") {}

    table->vkFreeCommandBuffers(table.device(), vkPool, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data());
}

void BaseCmdBuffer::begin(VkCommandBufferBeginInfo const& info) {
    debug_check(*this);
    CTH_CRITICAL(recording(), "command buffer must not be recording") {}

    auto const result = core().functions()->vkBeginCommandBuffer(get(), &info);
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to begin command buffer") {}

    _recording = true;
}

void BaseCmdBuffer::forget() { _recording = false; }

void BaseCmdBuffer::pool_deleter::operator()(VkCommandBuffer vkBuffer) const {
    --_pool->_buffers;
    destroy(_pool->core().deviceTable(), _pool->get(), vkBuffer);
}
}

namespace cvk {
PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool const& pool) : BaseCmdBuffer{pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY} {}

PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool const& pool, State const& state) : PrimaryCmdBuffer{pool} { wrap(state); }

PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool const& pool, create_t) : PrimaryCmdBuffer{pool} { create(); }

void PrimaryCmdBuffer::begin(VkCommandBufferUsageFlags usage) {
    VkCommandBufferBeginInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = usage,
        .pInheritanceInfo = nullptr,
    };
    BaseCmdBuffer::begin(info);
}

void PrimaryCmdBuffer::execute(span<SecondaryCmdBuffer const> secondaries) const {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    CTH_CRITICAL(
        std::ranges::any_of(
            secondaries,
            [](SecondaryCmdBuffer const& secondary) { return !secondary.created() || secondary.recording(); }
        ),
        "secondaries must be created and not recording"
    ) {}

    std::vector const vkBuffers{std::from_range, secondaries | std::views::transform(&SecondaryCmdBuffer::get)};

    core().functions()->vkCmdExecuteCommands(get(), static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data());
}
}

namespace cvk {
SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool const& pool) : BaseCmdBuffer{pool, VK_COMMAND_BUFFER_LEVEL_SECONDARY} {}

SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool const& pool, State const& state) : SecondaryCmdBuffer{pool} {
    wrap(state);
}

SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool const& pool, create_t) : SecondaryCmdBuffer{pool} { create(); }

void SecondaryCmdBuffer::begin(VkCommandBufferUsageFlags usage) {
    VkCommandBufferInheritanceInfo const inheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = nullptr,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .framebuffer = VK_NULL_HANDLE,
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = 0,
        .pipelineStatistics = 0,
    };
    VkCommandBufferBeginInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = usage,
        .pInheritanceInfo = &inheritanceInfo,
    };
    BaseCmdBuffer::begin(info);
}

void SecondaryCmdBuffer::begin(Inheritance const& inheritance, VkCommandBufferUsageFlags usage) {
    auto const renderingInfo = inheritance.renderingInfo();
    VkCommandBufferInheritanceInfo const inheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = &renderingInfo,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .framebuffer = VK_NULL_HANDLE,
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = 0,
        .pipelineStatistics = 0,
    };
    VkCommandBufferBeginInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = usage | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        .pInheritanceInfo = &inheritanceInfo,
    };
    BaseCmdBuffer::begin(info);
}
}
