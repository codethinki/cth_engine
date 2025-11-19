#pragma once
#include "cmd_buffer_config.hpp"

#include "jvk/base/device_table.hpp"
#include "jvk/utility/constants.hpp"
#include "jvk/utility/types.hpp"

#include <cth/coro/task.hpp>
#include <cth/pointer/move_ptr.hpp>

#include <volk.h>

#include <span>



namespace jvk {
class Framebuffer;
class Subpass;
class RenderPass;
class Core;

class Device;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;

class CmdPool;


class CmdBuffer {
public:
    using Config = CmdBufferConfig;

    /**
     * @brief base constructor
     */
    explicit CmdBuffer(Config config);
    virtual ~CmdBuffer() = default;

    void create(this auto&& self, CmdPool& pool);

    /**
     * @brief returns the command buffer to pool
     * @attention @ref created() required
     */
    template<class Me>
    void destroy(this Me&& self);

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy(this auto&& self) { if(self.created()) self.destroy(); }

    void end();

    void reset(VkCommandBufferResetFlags flags);


    static void destroy(DeviceTable table, VkCommandPool vk_pool, std::span<VkCommandBuffer const> buffers);
    static void destroy(DeviceTable table, vk_not_null<VkCommandPool_T*> vk_pool, VkCommandBuffer buffer);

protected:
    void begin(VkCommandBufferBeginInfo const& info);

private:
    void reset();


    Config _config;
    std::optional<DeviceTable> _deviceTable = std::nullopt;

    CmdPool* _pool = nullptr;
    move_ptr<VkCommandBuffer_T> _handle = VK_NULL_HANDLE;
    bool _recording = false;

    friend CmdPool;

public:
    [[nodiscard]] VkCommandBuffer get() const { return _handle.get(); }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] bool recording() const { return _recording; }
    [[nodiscard]] CmdPool& pool() const { return *_pool; }
    [[nodiscard]] VkBufferUsageFlags usageFlags() const { return _config.usageFlags; }
    [[nodiscard]] DeviceTable const& deviceTable() const;


    CmdBuffer(CmdBuffer const& other) = delete;
    CmdBuffer& operator=(CmdBuffer const& other) = delete;
    CmdBuffer(CmdBuffer&& other) = default;
    CmdBuffer& operator=(CmdBuffer&& other) = default;


    static void debug_check(CmdBuffer const& cmd_buffer);
    static void debug_check_handle(jvk::vk_not_null<VkCommandBuffer> handle);
};

inline void CmdBuffer::debug_check(CmdBuffer const& cmd_buffer) {
    CTH_CRITICAL(!cmd_buffer.created(), "cmd_buffer must be created") {}
}

inline void CmdBuffer::debug_check_handle([[maybe_unused]] jvk::vk_not_null<VkCommandBuffer> handle) {}

}

//PrimaryCmdBuffer

namespace jvk {
class PrimaryCmdBuffer : public CmdBuffer {
public:
    explicit PrimaryCmdBuffer(Config config) : CmdBuffer{config} {}
    explicit PrimaryCmdBuffer(Config config, CmdPool& cmd_pool);

    ~PrimaryCmdBuffer() override { optDestroy(); }

    void begin();



    PrimaryCmdBuffer(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer(PrimaryCmdBuffer&& other) noexcept = default;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer&& other) noexcept = default;
};
}

//SecondaryCmdBuffer

namespace jvk {
class SecondaryCmdBuffer : public CmdBuffer {
public:
    explicit SecondaryCmdBuffer(Config config) : CmdBuffer{std::move(config)} {}
    explicit SecondaryCmdBuffer(Config config, CmdPool& cmd_pool);

    ~SecondaryCmdBuffer() override { optDestroy(); }


    void begin(RenderPass const& render_pass, Subpass const& subpass, Framebuffer const* framebuffer);

private:
    VkCommandBufferInheritanceInfo _inheritanceInfo{};

public:
    SecondaryCmdBuffer(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer(SecondaryCmdBuffer&& other) noexcept = default;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer&& other) noexcept = default;
};
}
