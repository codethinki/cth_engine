#include "Phase.hpp"

#include "src/vulkan/render/control/CthSemaphore.hpp"


namespace cth::vk {


Phase::Phase(Core const& core, std::vector<Stage::Config> config) : _core{&core} { createSignalSemaphores(); }
void Phase::addWaitGroup(std::span<Stage::Config> configs) {
    for(auto& config : configs) {
      //  config.addWaitGroup();

        //TEMP left off here, add a "general timeline (normal wont work) wait group" param to the Phase config and add that to the stages
        //TEMP also add the signal semaphores to each stage and make one that just submits to wait on all the previous signal semaphores to sync to them

    }
    
}
void Phase::createSignalSemaphores() {
    size_t const semaphores = Stage::Config::GROUP_SIZE * _stages.size();

    _semaphores.reserve(semaphores);

    for(size_t i = 0; i < semaphores; ++i)
        _semaphores.emplace_back(*_core, vk::create);
}
std::vector<Stage*> Phase::stages() {
    std::vector<Stage*> stages(_stages.size());
    std::ranges::transform(_stages, stages.begin(), [](auto& stage) { return &stage; });
    return stages;
}
}
