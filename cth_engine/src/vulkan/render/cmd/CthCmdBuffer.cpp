#include "CthCmdBuffer.hpp"

#include "CthCmdPool.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


namespace cth::vk {
CmdBuffer::CmdBuffer(CmdPool* pool, VkCommandBufferUsageFlags  usage) : _pool(pool), _bufferUsage(usage) {}



void CmdBuffer::reset(VkCommandBufferResetFlags  flags) const {
    auto const result = vkResetCommandBuffer(_handle.get(), flags);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset command buffer")
        throw vk::result_exception{result, details->exception()};
}
void CmdBuffer::end() const {
    auto const result = vkEndCommandBuffer(_handle.get());
     CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset end buffer")
        throw vk::result_exception{result, details->exception()};
}

void CmdBuffer::free(VkDevice device, VkCommandPool vk_pool, std::span<VkCommandBuffer const> buffers) {
    DEBUG_CHECK_DEVICE_HANDLE(device);
    bool const valid = std::ranges::all_of(buffers, [](auto buffer) { return static_cast<bool>(buffer); });
    CTH_WARN(!valid, "> 0 vk_buffers invalid (VK_NULL_HANDLE)") {}
    CTH_ERR(valid && vk_pool == VK_NULL_HANDLE, "vk_pool is invalid (VK_NULL_HANDLE)")
        throw details->exception();

    vkFreeCommandBuffers(device, vk_pool, static_cast<uint32_t>(buffers.size()), buffers.data());
}
void CmdBuffer::free(VkDevice device, VkCommandPool vk_pool, VkCommandBuffer buffer) {
    DEBUG_CHECK_DEVICE_HANDLE(device);
    bool const valid = buffer != VK_NULL_HANDLE;
    CTH_WARN(!valid, "vk_buffer is invalid (VK_NULL_HANDLE)") {}
    CTH_ERR(valid && vk_pool != VK_NULL_HANDLE, "vk_pool is invalid (VK_NULL_HANDLE)")
        throw details->exception();


    vkFreeCommandBuffers(device, vk_pool, 1, &buffer);
}
void CmdBuffer::begin( VkCommandBufferBeginInfo const& info) const {
    auto const result = vkBeginCommandBuffer(_handle.get(), &info);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to begin command buffer")
        throw vk::result_exception{result, details->exception()}; }

#ifdef CONSTANT_DEBUG_MODE
void CmdBuffer::debug_check(CmdBuffer const* cmd_buffer) {
    CTH_ERR(cmd_buffer == nullptr, "cmd_buffer is invalid (nullptr)") throw details->exception();
    CTH_ERR(cmd_buffer->_handle == VK_NULL_HANDLE, "cmd_buffer handle is invalid (VK_NULL_HANDLE)") throw details->exception();
}

#endif


}

//PrimaryCmdBuffer

namespace cth::vk {

PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool* cmd_pool, VkCommandBufferUsageFlags  usage) : CmdBuffer(cmd_pool, usage) { create(); }
PrimaryCmdBuffer::~PrimaryCmdBuffer() { _pool->returnCmdBuffer(this); }
void PrimaryCmdBuffer::begin() const {
    VkCommandBufferBeginInfo const info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        _bufferUsage,
        nullptr,
    };
    CmdBuffer::begin(info);
}

void PrimaryCmdBuffer::create() { _pool->newCmdBuffer(this); }

}

//SecondaryCmdBuffer

namespace cth::vk {
SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool* cmd_pool, PrimaryCmdBuffer* primary, Config const& config, VkCommandBufferUsageFlags  usage) :
    CmdBuffer(cmd_pool, usage), _primary(primary), _inheritanceInfo(config.inheritanceInfo()) { create(); }
SecondaryCmdBuffer::~SecondaryCmdBuffer() { _pool->returnCmdBuffer(this); }

void SecondaryCmdBuffer::begin() const {
    VkCommandBufferBeginInfo const info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        _bufferUsage,
        &_inheritanceInfo,
    };
    CmdBuffer::begin(info);
}

void SecondaryCmdBuffer::create() { _pool->newCmdBuffer(this); }


} // namespace cth
