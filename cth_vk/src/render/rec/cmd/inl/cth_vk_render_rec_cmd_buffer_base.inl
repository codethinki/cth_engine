#pragma once
import cth.typ.utility;

import cth.vk.exception;

namespace cth::vk {
inline CmdBuffer::CmdBuffer(VkCommandBufferUsageFlags usage) : _bufferUsage{usage} {}

template<class Me>
void CmdBuffer::destroy(this Me&& self) {
    self.reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    self._pool->returnCmdBuffer(Me::TYPE, self._handle.get());

    self.reset();
}



inline void CmdBuffer::reset(VkCommandBufferResetFlags flags) {
    auto const result = _deviceTable->table->vkResetCommandBuffer(_handle.get(), flags);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset command buffer")
        throw vk::result_exception{result, details->exception()};

    _recording = false;
}
inline void CmdBuffer::end() {
    CTH_CRITICAL(!recording(), "cmd buffer must be in recording state"){}

    auto const result = _deviceTable->table->vkEndCommandBuffer(_handle.get());
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset end buffer")
        throw vk::result_exception{result, details->exception()};

    _recording = false;
}

inline void CmdBuffer::destroy(DeviceTable table, VkCommandPool vk_pool, std::span<VkCommandBuffer const> buffers) {
    bool const valid = std::ranges::all_of(buffers, [](auto buffer) { return static_cast<bool>(buffer); });
    CTH_WARN(!valid, "> 0 vk_buffers invalid (VK_NULL_HANDLE)") {}
    CTH_ERR(valid && vk_pool == VK_NULL_HANDLE, "vk_pool is invalid (VK_NULL_HANDLE)")
        throw details->exception();

    table->vkFreeCommandBuffers(table.device(), vk_pool, static_cast<uint32_t>(buffers.size()), buffers.data());
}
inline void CmdBuffer::destroy(DeviceTable table, vk::not_null<VkCommandPool> vk_pool, VkCommandBuffer buffer) {
    CTH_WARN(buffer == VK_NULL_HANDLE, "vk_buffer is invalid (VK_NULL_HANDLE)") {}


    table->vkFreeCommandBuffers(table.device(), vk_pool.get(), 1, &buffer);
}

void CmdBuffer::create(this auto&& self, CmdPool& pool) {
    self.optDestroy();
    self._pool = &pool;
    self._deviceTable = pool.core().deviceTable();
    auto const handle = self._pool->template newCmdBuffer<type::pure_t<decltype(self)>>();
    CTH_CRITICAL(handle == VK_NULL_HANDLE, "failed to create cmd buffer") {}

    self._handle = handle;
}

inline void CmdBuffer::begin(VkCommandBufferBeginInfo const& info) {
    auto const result = _pool->core().deviceTable()->vkBeginCommandBuffer(_handle.get(), &info);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to begin command buffer")
        throw vk::result_exception{result, details->exception()};

    _recording = true;
}

inline void CmdBuffer::reset() {
    _deviceTable = std::nullopt;
    _pool = nullptr;
    _handle = VK_NULL_HANDLE;
}

}
