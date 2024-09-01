#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include<cth/cth_pointer.hpp>
#include <vulkan/vulkan.h>

#include <span>
#include <type_traits>


namespace cth::vk {

class Device;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;
template<typename T>
concept cmd_buffer_t = std::_Is_any_of_v<std::decay_t<T>, PrimaryCmdBuffer, SecondaryCmdBuffer>;

class CmdPool;

class CmdBuffer {
public:
    explicit CmdBuffer(CmdPool* pool, VkCommandBufferUsageFlags usage = 0);
    virtual ~CmdBuffer() = 0;

    void reset(VkCommandBufferResetFlags flags = 0) const;
    virtual void begin() const = 0;

    [[nodiscard]] void end() const;

    static void free(VkDevice device, VkCommandPool vk_pool, std::span<VkCommandBuffer const> buffers);
    static void free(VkDevice device, VkCommandPool vk_pool, VkCommandBuffer buffer);

protected:
    void begin(VkCommandBufferBeginInfo const& info) const;

    move_ptr<CmdPool> _pool;

    move_ptr<VkCommandBuffer_T> _handle = VK_NULL_HANDLE;

    VkCommandBufferUsageFlags _bufferUsage;

private:
    friend CmdPool;

public:
    [[nodiscard]] VkCommandBuffer get() const { return _handle.get(); }

    CmdBuffer(CmdBuffer const& other) = delete;
    CmdBuffer& operator=(CmdBuffer const& other) = delete;
    CmdBuffer(CmdBuffer&& other) = default;
    CmdBuffer& operator=(CmdBuffer&& other) = default;


#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(CmdBuffer const* cmd_buffer);

#define DEBUG_CHECK_CMD_BUFFER(cmd_buffer_ptr) CmdBuffer::debug_check(cmd_buffer_ptr)
#else
#define DEBUG_CHECK_CMD_BUFFER(cmd_buffer_ptr) ((void)0)
#endif

};
inline CmdBuffer::~CmdBuffer() = default;

}

//PrimaryCmdBuffer

namespace cth::vk {
class PrimaryCmdBuffer : public CmdBuffer {
public:
    explicit PrimaryCmdBuffer(CmdPool* cmd_pool, VkCommandBufferUsageFlags usage = 0);
    ~PrimaryCmdBuffer() override;
   void begin() const override;

private:
    void create();
};
}

//SecondaryCmdBuffer

namespace cth::vk {
class SecondaryCmdBuffer : public CmdBuffer {
public:
    struct Config;
    SecondaryCmdBuffer(CmdPool* cmd_pool, PrimaryCmdBuffer* primary, Config const& config, VkCommandBufferUsageFlags usage = 0);
    ~SecondaryCmdBuffer() override;

    void begin() const override;

private:
    void create();

    PrimaryCmdBuffer* _primary;
    VkCommandBufferInheritanceInfo _inheritanceInfo;

public:
    struct Config {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        uint32_t subpassIndex = 0;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;


        VkBool32 occlusionQueryEnable = VK_FALSE;
        VkQueryControlFlags queryFlags = 0;
        VkQueryPipelineStatisticFlags pipelineStatistics = 0;


        static auto Default(VkRenderPass render_pass, uint32_t  subpass_index, VkFramebuffer framebuffer) {
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
};
}
