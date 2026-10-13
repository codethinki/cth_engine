#include "jolly/data/staging_allocator.hpp"

#include "jolly/core/core.hpp"
#include "jolly/render/submit/queue.hpp"
#include "jvk/data/buffer/base_buffer.hpp"

namespace jly {

StagingAllocator::StagingAllocator(Core const& core, Queue const& queue) :
    _core{&core},
    _queue{&queue},
    _buffer{
        std::make_unique<jvk::BaseBuffer>(
            _core->raw(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        )
    } {}
StagingAllocator::StagingAllocator(Core const& core, Queue const& queue, size_t size) : StagingAllocator{core,
    queue} { create(size); }
StagingAllocator::~StagingAllocator() = default;
void StagingAllocator::create(size_t size) {
    cxpr VkMemoryPropertyFlags flags{
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    VK_MEMORY_PROPERTY_HOST

    _buffer->create(size, flags);
}
void StagingAllocator::destroy() {
    debug_check(*this);

    _buffer->destroy();
    reset();
}
void StagingAllocator::optDestroy() {
    if(created())
        destroy();
}


bool StagingAllocator::created() const { return _buffer->created(); }
void StagingAllocator::reset() {}
size_t StagingAllocator::size() const { return _buffer->size(); }
}
