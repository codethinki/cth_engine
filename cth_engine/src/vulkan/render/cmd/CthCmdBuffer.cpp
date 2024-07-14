#include "CthCmdBuffer.hpp"

#include "CthCmdPool.hpp"
#include "vulkan/base/CthDevice.hpp"


namespace cth {
CmdBuffer::CmdBuffer(CmdPool* pool, const VkCommandBufferUsageFlags usage) : _pool(pool), _bufferUsage(usage) {}

VkResult CmdBuffer::reset() const { return vkResetCommandBuffer(_handle.get(), 0); }
VkResult CmdBuffer::reset(const VkCommandBufferResetFlags flags) const { return vkResetCommandBuffer(_handle.get(), flags); }
VkResult CmdBuffer::end() const { return vkEndCommandBuffer(_handle.get()); }

void CmdBuffer::free(VkDevice device, VkCommandPool vk_pool, std::span<const VkCommandBuffer> buffers) {
    DEBUG_CHECK_DEVICE_HANDLE(device);
    const bool valid = std::ranges::all_of(buffers, [](auto buffer) { return static_cast<bool>(buffer); });
    CTH_WARN(!valid, "> 0 vk_buffers invalid (VK_NULL_HANDLE)") {}
    CTH_ERR(valid && vk_pool == VK_NULL_HANDLE, "vk_pool is invalid (VK_NULL_HANDLE)")
        throw details->exception();

    vkFreeCommandBuffers(device, vk_pool, static_cast<uint32_t>(buffers.size()), buffers.data());
}
void CmdBuffer::free(VkDevice device, VkCommandPool vk_pool, VkCommandBuffer buffer) {
    DEBUG_CHECK_DEVICE_HANDLE(device);
    const bool valid = buffer != VK_NULL_HANDLE;
    CTH_WARN(!valid, "vk_buffer is invalid (VK_NULL_HANDLE)") {}
    CTH_ERR(valid && vk_pool != VK_NULL_HANDLE, "vk_pool is invalid (VK_NULL_HANDLE)")
        throw details->exception();


    vkFreeCommandBuffers(device, vk_pool, 1, &buffer);
}

#ifdef CONSTANT_DEBUG_MODE
void CmdBuffer::debug_check(const CmdBuffer* cmd_buffer) {
    CTH_ERR(cmd_buffer == nullptr, "cmd_buffer is invalid (nullptr)") throw details->exception();
    CTH_ERR(cmd_buffer->_handle == VK_NULL_HANDLE, "cmd_buffer handle is invalid (VK_NULL_HANDLE)") throw details->exception();
}

#endif


}

//PrimaryCmdBuffer

namespace cth {

PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool* cmd_pool, const VkCommandBufferUsageFlags usage) : CmdBuffer(cmd_pool, usage) { create(); }
PrimaryCmdBuffer::~PrimaryCmdBuffer() { _pool->returnCmdBuffer(this); }
VkResult PrimaryCmdBuffer::begin() const {
    const VkCommandBufferBeginInfo info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        _bufferUsage,
        nullptr,
    };
    return vkBeginCommandBuffer(_handle.get(), &info);
}

void PrimaryCmdBuffer::create() { _pool->newCmdBuffer(this); }

}

//SecondaryCmdBuffer

namespace cth {
SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool* cmd_pool, PrimaryCmdBuffer* primary, const Config& config, const VkCommandBufferUsageFlags usage) :
    CmdBuffer(cmd_pool, usage), _primary(primary), _inheritanceInfo(config.inheritanceInfo()) { create(); }
SecondaryCmdBuffer::~SecondaryCmdBuffer() { _pool->returnCmdBuffer(this); }

VkResult SecondaryCmdBuffer::begin() const {
    const VkCommandBufferBeginInfo info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        _bufferUsage,
        &_inheritanceInfo,
    };
    return vkBeginCommandBuffer(_handle.get(), &info);
}

void SecondaryCmdBuffer::create() { _pool->newCmdBuffer(this); }


} // namespace cth
