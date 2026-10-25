#pragma once
#include "cmd_allocator.hpp"
#include "cmd_buffer.hpp"

#include "cvk/base/queue/queue_family.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"

#include <cth/data/hash_map.hpp>
#include <cth/ptr/not_null.hpp>

#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>


namespace cvk {
class Core;
}

namespace jvk::dev {
/**
 * command pool of a @ref CmdAllocatorPool with its command buffers
 */
struct CmdPoolEntry {
    cvk::CmdPool pool;

    // destroyed before the pool
    std::deque<PrimaryCmdBuffer> primaries{};
    std::deque<SecondaryCmdBuffer> secondaries{};

    /**
     * command buffers in use since the last reset, the buffers after them are available
     */
    size_t usedPrimaries = 0;
    size_t usedSecondaries = 0;

    /**
     * an allocator holds the pool
     */
    bool held = false;
};
}

namespace jvk {
/**
 * command pools by (queue family, index), hands them to recordings as @ref CmdAllocator,
 *  neither copyable nor movable
 * @details role of the D3D12 command allocator pool,
 *  the index is the convention of the caller, the stage graph uses the frame slot,
 *  a pool keeps its command buffers, @ref reset() makes them available again,
 *  the pools of a key grow with the number of allocators at the same time, not with the number of threads
 * @attention thread safe
 */
class CmdAllocatorPool {
public:
    /**
     * @pre @ref cvk::Core::created()
     */
    explicit CmdAllocatorPool(cvk::Core const&);

    /**
     * @return allocator with a pool of the key that no other allocator holds, a new pool if every pool is held
     * @pre the core has a queue of the family
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
     */
    [[nodiscard]] CmdAllocator acquire(cvk::queue_family_index_t family, size_t index);

    /**
     * resets every pool of the key, their command buffers return to the initial state and are available again
     * @pre no allocator of the key exists
     * @pre the gpu finished every submit of their command buffers
     * @pre the submits and the primaries took the tasks of the command buffers
     * @throws cvk::vk_result_exception result of vkResetCommandPool()
     */
    void reset(cvk::queue_family_index_t family, size_t index);

private:
    friend class CmdAllocator;

    using key_t = std::pair<cvk::queue_family_index_t, size_t>;

    /**
     * marks the pool as not held, the end of its allocator calls it
     */
    void giveBack(dev::CmdPoolEntry&);

    cth::not_null_ptr<cvk::Core const> _core;
    std::mutex _mutex{};

    // a rehash moves the deques, not their entries: the allocators keep pointers to the entries
    cth::dt::flat_hash_map<key_t, std::deque<dev::CmdPoolEntry>> _entries{};

public:
    CmdAllocatorPool(CmdAllocatorPool const& other) = delete;
    CmdAllocatorPool(CmdAllocatorPool&& other) = delete;
    CmdAllocatorPool& operator=(CmdAllocatorPool const& other) = delete;
    CmdAllocatorPool& operator=(CmdAllocatorPool&& other) = delete;
};
}
