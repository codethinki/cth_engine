#include "jvk/render/cmd/cmd_buffer.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/device_table.hpp"
#include "jvk/render/cmd/cmd_pool.hpp"
#include "jvk/render/pass/render_pass.hpp"
#include "jvk/render/pass/subpass.hpp"
#include "jvk/render/pass/framebuffer/framebuffer.hpp"
#include "jvk/utility/vk_exceptions.hpp"


namespace jvk {
CmdBuffer::CmdBuffer(VkCommandBufferUsageFlags usage) : _bufferUsage{usage} {}

template<class Me>
void CmdBuffer::destroy(this Me&& self) {
    self.reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    static_assert(cth::type::is_any_of<cth::type::pure_t<Me>, PrimaryCmdBuffer, SecondaryCmdBuffer>);

    self._pool->template returnCmdBuffer<cth::type::pure_t<Me>>(self._handle.get());

    self.reset();
}



void CmdBuffer::reset(VkCommandBufferResetFlags flags) {
    auto const result = _deviceTable->table->vkResetCommandBuffer(_handle.get(), flags);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset command buffer")
    throw jvk::vk_result_exception{result, details->exception()};

    _recording = false;
}

void CmdBuffer::end() {
    CTH_CRITICAL(!recording(), "cmd buffer must be in recording state") {}

    auto const result = _deviceTable->table->vkEndCommandBuffer(_handle.get());
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset end buffer")
    throw jvk::vk_result_exception{result, details->exception()};

    _recording = false;
}

void CmdBuffer::destroy(DeviceTable table, VkCommandPool vk_pool, std::span<VkCommandBuffer const> buffers) {
    bool const valid = std::ranges::all_of(buffers, [](auto buffer) { return static_cast<bool>(buffer); });
    CTH_WARN(!valid, "> 0 vk_buffers invalid (VK_NULL_HANDLE)") {}
    CTH_ERR(valid && vk_pool == VK_NULL_HANDLE, "vk_pool is invalid (VK_NULL_HANDLE)")
    throw details->exception();

    table->vkFreeCommandBuffers(table.device(), vk_pool, static_cast<uint32_t>(buffers.size()),
        buffers.data());
}

void CmdBuffer::destroy(DeviceTable table, jvk::vk_not_null<VkCommandPool> vk_pool, VkCommandBuffer buffer) {
    CTH_WARN(buffer == VK_NULL_HANDLE, "vk_buffer is invalid (VK_NULL_HANDLE)") {}


    table->vkFreeCommandBuffers(table.device(), vk_pool.get(), 1, &buffer);
}

void CmdBuffer::create(this auto&& self, CmdPool& pool) {
    self.optDestroy();
    self._pool = &pool;
    self._deviceTable = pool.core().deviceTable();
    auto const handle = self._pool->template newCmdBuffer<cth::type::pure_t<decltype(self)>>();
    CTH_CRITICAL(handle == VK_NULL_HANDLE, "failed to create cmd buffer") {}

    self._handle = handle;
}

void CmdBuffer::begin(VkCommandBufferBeginInfo const& info) {
    auto const result = _pool->core().deviceTable()->vkBeginCommandBuffer(_handle.get(), &info);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to begin command buffer")
    throw jvk::vk_result_exception{result, details->exception()};

    _recording = true;
}

void CmdBuffer::reset() {
    _deviceTable = std::nullopt;
    _pool = nullptr;
    _handle = VK_NULL_HANDLE;

}

}


//PrimaryCmdBuffer

namespace jvk {

PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool& cmd_pool, VkCommandBufferUsageFlags usage) : CmdBuffer{usage} {
    create(cmd_pool);
}

void PrimaryCmdBuffer::begin() {
    VkCommandBufferBeginInfo const info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        usageFlags(),
        nullptr,
    };
    CmdBuffer::begin(info);
}


}


//SecondaryCmdBuffer

namespace jvk {
SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool& cmd_pool,
    VkCommandBufferUsageFlags usage) : SecondaryCmdBuffer{usage} { create(cmd_pool); }

void SecondaryCmdBuffer::begin(RenderPass const& render_pass, Subpass const& subpass,
    Framebuffer const* framebuffer) {
    _inheritanceInfo.renderPass = render_pass.get();
    _inheritanceInfo.subpass = subpass.index();
    _inheritanceInfo.framebuffer = framebuffer != nullptr ? framebuffer->get() : VK_NULL_HANDLE;

    VkCommandBufferBeginInfo const info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        usageFlags(),
        &_inheritanceInfo,
    };
    CmdBuffer::begin(info);
}


} // namespace cth
