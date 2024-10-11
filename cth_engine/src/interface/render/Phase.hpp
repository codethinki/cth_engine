#pragma once
#include "CthStage.hpp"


namespace cth::vk {
class Core;
class Semaphore;

class Phase {
public:
    struct Config;
    Phase(cth::not_null<Core const*> core, std::span<Stage::Config> config);


    PrimaryCmdBuffer* begin(size_t cycle_sub_index);
    void end(size_t cycle_sub_index);



private:
    std::vector<Stage> _stages;
    std::vector<Semaphore> _semaphores;
};
}


namespace cth::vk {
struct Phase::Config {
    std::vector<Stage::Config> stages;
};
} //namespace cth::vk
