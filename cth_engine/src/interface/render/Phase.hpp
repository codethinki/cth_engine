#pragma once
#include "CthStage.hpp"


namespace cth::vk {
class Core;
class Semaphore;

class Phase {
public:
    struct Config;
    Phase(Core const& core, std::vector<Stage::Config> config);


    // PrimaryCmdBuffer* begin(size_t cycle_sub_index); TEMP remove
    // void end(size_t cycle_sub_index);

    void wait();

    void skip();

private:
    void addWaitGroup(std::span<Stage::Config> configs);

    void createSignalSemaphores();

    cth::not_null<Core const*> _core;

    std::vector<Stage> _stages;
    std::vector<Semaphore> _semaphores;

public:
    std::vector<Stage*> stages();
};
}


namespace cth::vk {
struct Phase::Config {
    std::vector<Stage::Config> stages;
};
} //namespace cth::vk
