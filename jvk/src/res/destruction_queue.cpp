#include "jvk/base/destruction_queue.hpp"

#include "jvk/debug/debug_messenger.hpp"
#include "jvk/res/buffer/base_buffer.hpp"


namespace jvk {


DestructionQueue::DestructionQueue(Config config) : _queue{config.delay} {}
DestructionQueue::~DestructionQueue() { clear(); }

void DestructionQueue::push(function_t const& function) { _queue[_cycleSubIndex].emplace_back(function); }

void DestructionQueue::push(std::span<function_t const> functions) { for(auto& function : functions) push(function); }



void DestructionQueue::clearQueue() {
    auto& deletables = _queue[_cycleSubIndex];
    for(auto& deletable : deletables) deletable();
    deletables.clear();
}

void DestructionQueue::clear() { for(uint32_t i = 0; i < _queue.size(); ++i) next(); }

void DestructionQueue::next() {
    ++_cycleSubIndex %= _queue.size();
    clearQueue();
}

}
