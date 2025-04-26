module;
#include <cth/macro.hpp>

export module cth.vk.render.rec.cmd.buffer.secondary;
import cth.vk.render.rec.cmd.buffer.base;
import cth.vk.render.rec.cmd.pool;
import cth.vk.util.types;

//SecondaryCmdBuffer

export namespace cth::vk {
class SecondaryCmdBuffer : public CmdBuffer {
public:
    static cxpr CmdPool::BufferType TYPE = CmdPool::BUFFER_TYPE_SECONDARY;


    explicit SecondaryCmdBuffer(VkCommandBufferUsageFlags usage = 0) : CmdBuffer{usage} {}
    explicit SecondaryCmdBuffer(CmdPool& cmd_pool, VkCommandBufferUsageFlags usage = 0): SecondaryCmdBuffer{usage} { create(cmd_pool); }

    ~SecondaryCmdBuffer() override { optDestroy(); }


    void begin(vk::not_null<VkRenderPass> render_pass, uint32_t subpass_index, VkFramebuffer framebuffer = VK_NULL_HANDLE) {
        _inheritanceInfo.renderPass = render_pass.get();
        _inheritanceInfo.subpass = subpass_index;
        _inheritanceInfo.framebuffer = framebuffer;

        VkCommandBufferBeginInfo const info{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            nullptr,
            usageFlags(),
            &_inheritanceInfo,
        };
        CmdBuffer::begin(info);
    }

private:
    VkCommandBufferInheritanceInfo _inheritanceInfo{};

public:
    SecondaryCmdBuffer(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer(SecondaryCmdBuffer&& other) noexcept = default;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer&& other) noexcept = default;
};
}
