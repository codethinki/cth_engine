#pragma once
#include <type_traits>
#include <vulkan/vulkan.h>


namespace cth {
using namespace std;

class PrimaryCmdBuffer;
class SecondaryCmdBuffer;
template<typename T>
concept cmd_buffer_t = _Is_any_of_v<decay_t<T>, PrimaryCmdBuffer, SecondaryCmdBuffer>;

class CmdPool;

class CmdBuffer {
public:
    explicit CmdBuffer(CmdPool* pool, VkCommandBufferUsageFlags usage = 0);
    virtual ~CmdBuffer() = 0;

    VkResult reset() const;
    VkResult reset(VkCommandBufferResetFlags flags) const;

    VkResult end() const;
    virtual VkResult begin() const = 0;

protected:
    CmdPool* pool;

    VkCommandBuffer vkBuffer = VK_NULL_HANDLE;

    VkCommandBufferUsageFlags bufferUsage;

private:
    friend CmdPool;

public:
    [[nodiscard]] auto get() const { return vkBuffer; }

    CmdBuffer(const CmdBuffer& other) = delete;
    CmdBuffer(CmdBuffer&& other) = delete;
    CmdBuffer& operator=(const CmdBuffer& other) = delete;
    CmdBuffer& operator=(CmdBuffer&& other) = delete;
};
inline CmdBuffer::~CmdBuffer() = default;

}

//PrimaryCmdBuffer

namespace cth {
class PrimaryCmdBuffer : public CmdBuffer {
public:
    explicit PrimaryCmdBuffer(CmdPool* cmd_pool, VkCommandBufferUsageFlags usage = 0);
    ~PrimaryCmdBuffer() override;
    VkResult begin() const override;

private:
    void create();
};
}

//SecondaryCmdBuffer

namespace cth {
class SecondaryCmdBuffer : public CmdBuffer {
public:
    struct Config;
    SecondaryCmdBuffer(CmdPool* cmd_pool, PrimaryCmdBuffer* primary, const Config& config, VkCommandBufferUsageFlags usage = 0);
    ~SecondaryCmdBuffer() override;

    VkResult begin() const override;

private:
    void create();

    PrimaryCmdBuffer* primary;
    VkCommandBufferInheritanceInfo inheritanceInfo;

public:
    struct Config {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        uint32_t subpassIndex = 0;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;


        VkBool32 occlusionQueryEnable = VK_FALSE;
        VkQueryControlFlags queryFlags = 0;
        VkQueryPipelineStatisticFlags pipelineStatistics = 0;


        static auto Default(VkRenderPass render_pass, const uint32_t subpass_index, VkFramebuffer framebuffer) {
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
