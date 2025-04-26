module;
#include <cth/macro.hpp>
export module cth.vk.render.rec.cmd.buffer.primary;

import cth.vk.render.rec.cmd.buffer.base;
import cth.vk.render.rec.cmd.pool;

export namespace cth::vk {
class PrimaryCmdBuffer : public CmdBuffer {
public:
    static cxpr CmdPool::BufferType TYPE = CmdPool::BUFFER_TYPE_PRIMARY;

    explicit PrimaryCmdBuffer(VkCommandBufferUsageFlags usage = 0) : CmdBuffer{usage} {}
    explicit PrimaryCmdBuffer(CmdPool& cmd_pool, VkCommandBufferUsageFlags usage = 0) : PrimaryCmdBuffer{usage} { create(cmd_pool); }

    ~PrimaryCmdBuffer() override { optDestroy(); }

    void begin() {
        VkCommandBufferBeginInfo const info{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            nullptr,
            usageFlags(),
            nullptr,
        };
        CmdBuffer::begin(info);
    }

    PrimaryCmdBuffer(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer(PrimaryCmdBuffer&& other) noexcept = default;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer&& other) noexcept = default;
};
}
