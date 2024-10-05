#pragma once
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

#include <cth/pointers.hpp>
#include <vulkan/vulkan.h>

#include <span>


namespace cth::vk {

class Device;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;

class CmdPool;


class CmdBuffer {
public:
    /**
     * @brief base constructor
     */
    explicit CmdBuffer(VkCommandBufferUsageFlags usage = 0);
    virtual ~CmdBuffer() = default;

    /**
     * @brief returns the command buffer to pool
     * @attention @ref created() required
     */
    void destroy(this auto&& self);

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy(this auto&& self) { if(self.created()) self.destroy(); }

    virtual void begin() const = 0;

    void end() const;

    void reset(VkCommandBufferResetFlags flags) const;



    static void destroy(VkDevice device, VkCommandPool vk_pool, std::span<VkCommandBuffer const> buffers);
    static void destroy(not_null<VkDevice_T*> device, not_null<VkCommandPool_T*> vk_pool, VkCommandBuffer buffer);

protected:
    void create(this auto&& self, cth::not_null<CmdPool*> pool);


    void begin(VkCommandBufferBeginInfo const& info) const;
    VkCommandBufferUsageFlags _bufferUsage;

private:
    void reset();

    CmdPool* _pool = nullptr;
    move_ptr<VkCommandBuffer_T> _handle = VK_NULL_HANDLE;


    friend CmdPool;

public:
    [[nodiscard]] VkBufferUsageFlags usageFlags() const { return _bufferUsage; }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkCommandBuffer get() const { return _handle.get(); }
    [[nodiscard]] CmdPool* pool() const { return _pool; }

    CmdBuffer(CmdBuffer const& other) = delete;
    CmdBuffer& operator=(CmdBuffer const& other) = delete;
    CmdBuffer(CmdBuffer&& other) = default;
    CmdBuffer& operator=(CmdBuffer&& other) = default;


    static void debug_check(cth::not_null<CmdBuffer const*> cmd_buffer);
    static void debug_check_handle(vk::not_null<VkCommandBuffer> handle);
};

inline void CmdBuffer::debug_check(cth::not_null<CmdBuffer const*> cmd_buffer) {
    CTH_CRITICAL(!cmd_buffer->created(), "cmd_buffer must be created") {}
}
inline void CmdBuffer::debug_check_handle([[maybe_unused]] vk::not_null<VkCommandBuffer> handle) {}

}

//PrimaryCmdBuffer

namespace cth::vk {
class PrimaryCmdBuffer : public CmdBuffer {
public:
    explicit PrimaryCmdBuffer(VkCommandBufferUsageFlags usage = 0) : CmdBuffer{usage} {}
    explicit PrimaryCmdBuffer(cth::not_null<CmdPool*> cmd_pool, VkCommandBufferUsageFlags usage = 0);

    ~PrimaryCmdBuffer() override;

    void begin() const override;
    void create(cth::not_null<CmdPool*> pool) { CmdBuffer::create(pool); }

    PrimaryCmdBuffer(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer(PrimaryCmdBuffer&& other) noexcept = default;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer&& other) noexcept = default;
};
}

//SecondaryCmdBuffer

namespace cth::vk {
class SecondaryCmdBuffer : public CmdBuffer {
public:
    struct Config;
    explicit SecondaryCmdBuffer(Config const& config, VkCommandBufferUsageFlags usage = 0);
    SecondaryCmdBuffer(cth::not_null<PrimaryCmdBuffer*> primary, Config const& config, VkCommandBufferUsageFlags usage = 0);


    ~SecondaryCmdBuffer() override { destroy(); }

    void begin() const override;

    void create(cth::not_null<PrimaryCmdBuffer*> primary);

private:
    PrimaryCmdBuffer* _primary = nullptr;
    VkCommandBufferInheritanceInfo _inheritanceInfo;

public:
    [[nodiscard]] auto primary() const { return _primary; }

    SecondaryCmdBuffer(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer(SecondaryCmdBuffer&& other) noexcept = default;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer&& other) noexcept = default;
};
}

//SecondaryCmdBuffer::Config

namespace cth::vk {
struct SecondaryCmdBuffer::Config {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpassIndex = 0;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;


    VkBool32 occlusionQueryEnable = VK_FALSE;
    VkQueryControlFlags queryFlags = 0;
    VkQueryPipelineStatisticFlags pipelineStatistics = 0;


    static auto Default(VkRenderPass render_pass, uint32_t subpass_index, VkFramebuffer framebuffer) {
        return Config{render_pass, subpass_index, framebuffer};
    }

private:
    [[nodiscard]] auto inheritanceInfo() const {
        return VkCommandBufferInheritanceInfo{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
            nullptr,
            renderPass,
            subpassIndex,
            framebuffer,

            occlusionQueryEnable,
            queryFlags,
            pipelineStatistics
        };
    }

    friend SecondaryCmdBuffer;
};
}
