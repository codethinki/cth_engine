#pragma once
#include "jolly/utility/types.hpp"

//TEMP left off here continue implementing this allocator
// use the ring allocator from the cth library. for multi queue support lockless ringbuffer with "finished" tickets and
// linear backwards search for the last ticket with assigning the cumulative free to lower ones if finished before lowerst
// make sure to support templated stuff, may need to have an ugly implementation like for the jvk::Buffer

namespace jvk {
class BaseBuffer;
}

namespace jly {
class Queue;
class Core;

/**
 * @attention assumes single queue usage
 * @details would work by default bc single queue submits are guaranteed to sync after one another
 */
class StagingAllocator {
public:
    StagingAllocator(Core const&, Queue const&);
    /**
     * constructs and creates
     * @param size to pass to create
     */
    StagingAllocator(Core const&, Queue const&, size_t size);
    ~StagingAllocator();

    /**
     * creates the buffer
     * @param size in bytes
     * @details calls @ref optDestroy()
     */
    void create(size_t size);

    /**
     * @pre @ref created()
     */
    void destroy();
    /**
     * if @ref created() calls @ref destroy()
     */
    void optDestroy();

    [[nodiscard]] bool created() const;

private:
    void reset();

    not_null<Core const*> _core;
    not_null<Queue const*> _queue;
    std::unique_ptr<jvk::BaseBuffer> _buffer;

public:
    /**
     * @pre @ref created()
     */
    [[nodiscard]] size_t size() const;

    static void debug_check(StagingAllocator const&);
};

}

namespace jly {
inline void StagingAllocator::debug_check(StagingAllocator const& a) {
    CTH_CRITICAL(!a.created(), "allocator must be created") {}
}
}
