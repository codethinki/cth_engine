#include "CthCmdBuffer.hpp"

#include "CthCmdPool.hpp"

namespace cth {
cth::CmdBuffer::CmdBuffer(CmdPool* pool, const VkCommandBufferUsageFlags usage) : pool(pool), bufferUsage(usage) {}

VkResult CmdBuffer::reset() const { return vkResetCommandBuffer(vkBuffer, 0); }
VkResult CmdBuffer::reset(const VkCommandBufferResetFlags flags) const { return vkResetCommandBuffer(vkBuffer, flags); }
VkResult CmdBuffer::end() const { return vkEndCommandBuffer(vkBuffer); }


}

//PrimaryCmdBuffer

namespace cth {

PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool* cmd_pool, const VkCommandBufferUsageFlags usage) : CmdBuffer(cmd_pool, usage) { create(); }
PrimaryCmdBuffer::~PrimaryCmdBuffer() {
    pool->returnCmdBuffer(this);
}
VkResult PrimaryCmdBuffer::begin() const {
    const VkCommandBufferBeginInfo info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        bufferUsage,
        nullptr,
    };
    return vkBeginCommandBuffer(vkBuffer, &info);
}

void PrimaryCmdBuffer::create() {
    pool->newCmdBuffer(this);
}

}

//SecondaryCmdBuffer

namespace cth {
SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool* cmd_pool, PrimaryCmdBuffer* primary, const Config& config, const VkCommandBufferUsageFlags usage) :
    CmdBuffer(cmd_pool, usage), primary(primary), inheritanceInfo(config.inheritanceInfo()) { create(); }
SecondaryCmdBuffer::~SecondaryCmdBuffer() {
    pool->returnCmdBuffer(this);
}

VkResult SecondaryCmdBuffer::begin() const {
    const VkCommandBufferBeginInfo info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        bufferUsage,
        &inheritanceInfo,
    };
    return vkBeginCommandBuffer(vkBuffer, &info);
}

void SecondaryCmdBuffer::create() { pool->newCmdBuffer(this); }


} // namespace cth
