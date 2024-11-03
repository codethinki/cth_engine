#include "Phase.hpp"


namespace cth::vk {


std::vector<Stage*> Phase::stages() {
    std::vector<Stage*> stages(_stages.size());
    std::ranges::transform(_stages, stages.begin(), [](auto& stage) { return &stage; });
    return stages;
}
}
