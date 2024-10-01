#include "CthCmdBuffer.hpp"

#include "CthCmdPool.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


namespace cth::vk {
CmdBuffer::CmdBuffer(VkCommandBufferUsageFlags usage) : _bufferUsage{usage} {}

void CmdBuffer::destroy(this auto&& self) {
    self.reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    static_assert(type::is_any_of<type::pure_t<decltype(self)>, PrimaryCmdBuffer, SecondaryCmdBuffer>);

    self._pool->template returnCmdBuffer<type::pure_t<decltype(self)>>(self._handle.get());

    self.reset();
}



void CmdBuffer::reset(VkCommandBufferResetFlags flags) const {
    auto const result = vkResetCommandBuffer(_handle.get(), flags);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset command buffer")
        throw vk::result_exception{result, details->exception()};
}
void CmdBuffer::end() const {
    auto const result = vkEndCommandBuffer(_handle.get());
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset end buffer")
        throw vk::result_exception{result, details->exception()};
}

void CmdBuffer::destroy(VkDevice device, VkCommandPool vk_pool, std::span<VkCommandBuffer const> buffers) {
    DEBUG_CHECK_DEVICE_HANDLE(device);
    bool const valid = std::ranges::all_of(buffers, [](auto buffer) { return static_cast<bool>(buffer); });
    CTH_WARN(!valid, "> 0 vk_buffers invalid (VK_NULL_HANDLE)") {}
    CTH_ERR(valid && vk_pool == VK_NULL_HANDLE, "vk_pool is invalid (VK_NULL_HANDLE)")
        throw details->exception();

    vkFreeCommandBuffers(device, vk_pool, static_cast<uint32_t>(buffers.size()), buffers.data());
}
void CmdBuffer::destroy(vk::not_null<VkDevice> device, vk::not_null<VkCommandPool> vk_pool, VkCommandBuffer buffer) {

    CTH_WARN(buffer == VK_NULL_HANDLE, "vk_buffer is invalid (VK_NULL_HANDLE)") {}


    vkFreeCommandBuffers(device.get(), vk_pool.get(), 1, &buffer);
}

void CmdBuffer::create(this auto&& self, cth::not_null<CmdPool*> pool) {
    self.optDestroy();

    self._pool = pool.get();
    self._handle = self._pool->template newCmdBuffer<type::pure_t<decltype(self)>>();
}

void CmdBuffer::begin(VkCommandBufferBeginInfo const& info) const {
    auto const result = vkBeginCommandBuffer(_handle.get(), &info);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to begin command buffer")
        throw vk::result_exception{result, details->exception()};
}

void CmdBuffer::reset() {
    _pool = nullptr;
    _handle = VK_NULL_HANDLE;

}

}

//PrimaryCmdBuffer

namespace cth::vk {

PrimaryCmdBuffer::PrimaryCmdBuffer(cth::not_null<CmdPool*> cmd_pool, VkCommandBufferUsageFlags usage) : CmdBuffer{usage} { create(cmd_pool); }

PrimaryCmdBuffer::~PrimaryCmdBuffer() { destroy(); }
void PrimaryCmdBuffer::begin() const {
    VkCommandBufferBeginInfo const info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        _bufferUsage,
        nullptr,
    };
    CmdBuffer::begin(info);
}


}

//SecondaryCmdBuffer

namespace cth::vk {
SecondaryCmdBuffer::SecondaryCmdBuffer(Config const& config, VkCommandBufferUsageFlags usage) : CmdBuffer{usage},
    _inheritanceInfo{config.inheritanceInfo()} {}
SecondaryCmdBuffer::SecondaryCmdBuffer(cth::not_null<PrimaryCmdBuffer*> primary, Config const& config,
    VkCommandBufferUsageFlags usage) : SecondaryCmdBuffer{config, usage} { create(primary); }

void SecondaryCmdBuffer::begin() const {
    VkCommandBufferBeginInfo const info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        usageFlags(),
        &_inheritanceInfo,
    };
    CmdBuffer::begin(info);
}
void SecondaryCmdBuffer::create(cth::not_null<PrimaryCmdBuffer*> primary) {
    _primary = primary.get();
    CmdBuffer::create(primary->pool());
}



} // namespace cth
