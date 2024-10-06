#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include <vulkan/vulkan.h>


namespace cth::vk {
struct PipelineWaitStage;
struct SubmitInfo;
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
    Phase(cth::not_null<Core*> core, Config const& config);

    void createSubmitInfos();

    cth::not_null<Core*> _core;
    std::unique_ptr<RenderPass> _renderPass;


    cth::not_null<Queue const*> _queue;
    std::vector<Semaphore*> _signalSemaphores;
    std::vector<PipelineWaitStage> _waitStages;


    VkExtent2D _extent{};
    std::vector<SubmitInfo> _submitInfos;
    std::unique_ptr<CmdPool> _cmdPool;
    std::vector<PrimaryCmdBuffer> _cmdBuffers;
};

}

//Config

namespace cth::vk {
struct Phase::Config {
    static constexpr auto GROUP_SIZE = constants::FRAMES_IN_FLIGHT;


    Config() = default;
    explicit Config(std::unique_ptr<RenderPass> render_pass, std::span<Semaphore const*> signal_groups = {}, std::span<PipelineWaitStage> wait_groups = {});


    Config& addSignalGroup(this Config& self, std::span<Semaphore const*> signal_group);
    Config& addWaitGroup(this Config& self, std::span<PipelineWaitStage const> wait_group);

    Config& addSignalGroups(this Config& self, std::span<Semaphore const*> signal_groups);
    Config& addWaitGroups(this Config& self, std::span<PipelineWaitStage> wait_groups);


private:
    std::unique_ptr<RenderPass> _renderPass;
    std::vector<Semaphore const*> _signalSemaphores{};
    std::vector<PipelineWaitStage> _waitStages{};

    friend class Phase;
};

}
