// ReSharper disable CppMemberFunctionMayBeStatic explicit this is not static
#include "CthPhase.hpp"

#include "vulkan/base/queue/CthSubmitInfo.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/render/control/CthWaitStage.hpp"

namespace cth::vk {
void Phase::createSubmitInfos() {
    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; ++i) {
        _submitInfos.emplace_back(
            std::vector{&_cmdBuffers[i]},
            std::vector{std::from_range, _waitStages | std::views::drop(i) | std::views::stride(constants::FRAMES_IN_FLIGHT)},
            std::vector{std::from_range, _signalSemaphores | std::views::drop(i) | std::views::stride(constants::FRAMES_IN_FLIGHT)},
            nullptr
            );
    }
}


}

//TEMP continue here (Phase class should represent the currennt renderer phases

namespace cth::vk {
Phase::Config::Config(std::unique_ptr<RenderPass> render_pass, std::span<Semaphore const*> signal_groups,
    std::span<PipelineWaitStage> wait_groups) : _renderPass{std::move(render_pass)} {
    addSignalGroups(signal_groups);
    addWaitGroups(wait_groups);
}

Phase::Config& Phase::Config::addSignalGroup(this Config& self, std::span<Semaphore const*> signal_group) {
    CTH_CRITICAL(signal_group.size() == Config::GROUP_SIZE, "signal_group size ({0}) == Config::GROUP_SIZE ({1}) required",
        signal_group.size(), Config::GROUP_SIZE){}

    for(auto const semaphore : signal_group) Semaphore::debug_check(semaphore);

    self._signalSemaphores.append_range(signal_group);
    return self;
}
Phase::Config& Phase::Config::addWaitGroup(this Config& self, std::span<PipelineWaitStage const> wait_group) {
    CTH_CRITICAL(wait_group.size() == Config::GROUP_SIZE, "signal_group size ({0}) == Config::GROUP_SIZE ({1}) required",
        wait_group.size(), Config::GROUP_SIZE){}

    for(auto const& waitStage : wait_group) PipelineWaitStage::debug_check(waitStage);

    self._waitStages.append_range(wait_group);
    return self;
}
Phase::Config& Phase::Config::addSignalGroups(this Config& self, std::span<Semaphore const*> signal_groups) {
    CTH_CRITICAL(signal_groups.size() % GROUP_SIZE == 0, "signal_groups.size() ({0}) % Config::GroupSize ({1}) == 0 required", signal_groups.size(),
        Config::GROUP_SIZE) {}
    auto const view = signal_groups | std::views::chunk(GROUP_SIZE);


    for(auto const signalGroup : view) self.addSignalGroup(signalGroup);
    return self;
}
Phase::Config& Phase::Config::addWaitGroups(this Config& self, std::span<PipelineWaitStage> wait_groups) {
    CTH_CRITICAL(wait_groups.size() % GROUP_SIZE == 0, "signal_groups.size() ({0}) % Config::GroupSize ({1}) == 0 required", wait_groups.size(),
        Config::GROUP_SIZE) {}
    auto const view = wait_groups | std::views::chunk(GROUP_SIZE);


    for(auto const waitGroup : view) self.addWaitGroup(waitGroup);
    return self;
}



}
