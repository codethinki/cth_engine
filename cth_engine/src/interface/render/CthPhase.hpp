#pragma once
#include "vulkan/base/queue/CthSubmitInfo.hpp"
#include "vulkan/render/control/CthWaitStage.hpp"

#include <vulkan/vulkan.h>


namespace cth::vk {
class CmdPool;
class Core;
class PrimaryCmdBuffer;
class Queue;
class RenderPass;
class Semaphore;

class Phase {
public:
    struct Config;

private:
    cth::not_null<Core*> _core;

    std::unique_ptr<RenderPass> _renderPass;
    cth::not_null<Queue const*> _queue;

    std::array<std::unique_ptr<PrimaryCmdBuffer>, constants::FRAMES_IN_FLIGHT> _cmdBuffers;
    std::unique_ptr<CmdPool> _cmdPool;

    std::vector<Semaphore const*> _signalSemaphores;
    std::vector<PipelineWaitStage> _waitStages;

    VkExtent2D _extent{};

    SubmitInfo _submitInfo;
};

}

//Config

namespace cth::vk {
struct Phase::Config {
    static constexpr auto GROUP_SIZE = constants::FRAMES_IN_FLIGHT;

    Config() = default;
    Config(std::span<Semaphore const*> signal_groups, std::span<PipelineWaitStage> wait_groups);


    Config& addSignalGroup(this Config& self, std::span<Semaphore const*> signal_group);
    Config& addWaitGroup(this Config& self, std::span<PipelineWaitStage const> wait_group);

    Config& addSignalGroups(this Config& self, std::span<Semaphore const*> signal_groups);
    Config& addWaitGroups(this Config& self, std::span<PipelineWaitStage> wait_groups);

private:
    std::vector<Semaphore const*> _signalSemaphores{};
    std::vector<PipelineWaitStage> _waitStages{};

    friend class Phase;
};

}
