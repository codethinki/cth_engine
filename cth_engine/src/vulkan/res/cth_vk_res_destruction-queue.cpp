module cth_vk_res_destruction_queue;

namespace cth::vk {


DestructionQueue::~DestructionQueue() { clear(); }

void DestructionQueue::push(function_t const& function) { _queue[_cycleSubIndex].emplace_back(function); }
void DestructionQueue::push(std::span<function_t const> functions) { for(auto& function : functions) push(function); }



void DestructionQueue::clearQueue() {
    auto& deletables = _queue[_cycleSubIndex];
    for(auto& deletable : deletables) deletable();
    deletables.clear();
}
void DestructionQueue::clear() {
    for(uint32_t i = 0; i < QUEUES; ++i) next();
}
void DestructionQueue::next() {
    ++_cycleSubIndex %= QUEUES;
    clearQueue();
}

}
