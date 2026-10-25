#pragma once
#include "cvk/render/cmd/cmd_buffer.hpp"

#include <volk.h>

#include <cth/ptr/not_null.hpp>

#include <memory>


namespace jvk {
class CmdAllocatorPool;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;
}

namespace jvk::dev {
struct CmdPoolEntry;
}

namespace jvk {
/**
 * command buffers of one recording, from a command pool that it holds alone, move only
 * @details role of the D3D12 command allocator,
 *  gives the pool back to its @ref CmdAllocatorPool at its end,
 *  its command buffers stay valid after its end until @ref CmdAllocatorPool::reset() of its key,
 *  a primary may execute secondaries of other allocators
 * @attention not thread safe: one thread at a time, it may move between threads
 */
class CmdAllocator {
public:
    /**
     * begins a primary command buffer of the pool, a new one if every buffer of the pool is in use
     * @param usage VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, ...
     * @details calls @ref PrimaryCmdBuffer::begin()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    [[nodiscard]] PrimaryCmdBuffer& primary(VkCommandBufferUsageFlags usage = 0);

    /**
     * begins a secondary command buffer of the pool, a new one if every buffer of the pool is in use
     * @details calls @ref SecondaryCmdBuffer::begin(Inheritance const&, VkCommandBufferUsageFlags)
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    [[nodiscard]] SecondaryCmdBuffer& secondary(cvk::SecondaryCmdBuffer::Inheritance const&);

private:
    friend class CmdAllocatorPool;

    /**
     * gives the pool back to its @ref CmdAllocatorPool
     */
    class pool_deleter {
    public:
        constexpr explicit pool_deleter(CmdAllocatorPool& pool) : _pool{&pool} {}

        void operator()(dev::CmdPoolEntry*) const;

    private:
        cth::not_null_ptr<CmdAllocatorPool> _pool;
    };

    /**
     * the pool constructs its allocators
     * @pre no other allocator holds the pool
     */
    CmdAllocator(CmdAllocatorPool&, dev::CmdPoolEntry&);

    std::unique_ptr<dev::CmdPoolEntry, pool_deleter> _entry;

public:
    CmdAllocator(CmdAllocator const& other) = delete;
    CmdAllocator(CmdAllocator&& other) noexcept = default;
    CmdAllocator& operator=(CmdAllocator const& other) = delete;
    CmdAllocator& operator=(CmdAllocator&& other) noexcept = default;
};
}
