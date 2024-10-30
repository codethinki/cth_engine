// ReSharper disable CppMemberFunctionMayBeStatic explicit this is not static
#include "CthStage.hpp"

#include "CthRenderCycle.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/queue/CthQueue.hpp"
#include "vulkan/base/queue/CthSubmitInfo.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/cmd/CthCmdPool.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/render/control/CthWaitStage.hpp"
#include "vulkan/render/pass/CthRenderPass.hpp"


namespace cth::vk {
struct Cycle;
void Stage::create() {
    createCmdPool();
    createCmdBuffers();
    createSubmitInfos();
}
void Stage::destroy() {}
void Stage::submit() { _queue->submit(current(_submitInfos)); }
void Stage::skip() {
    _queue->skip(current(_submitInfos));
}
PrimaryCmdBuffer* Stage::begin() {
    auto const buffer = &current(_cmdBuffers);
    buffer->begin();
    return buffer;
}
void Stage::end() { current(_cmdBuffers).end(); }


Stage::Stage(cth::not_null<Core*> core, Config config) : _core{core},
    _queue{std::move(config._queue)},
    _renderPass{std::move(config._renderPass)},
    _signalSemaphores{std::move(config._signalSemaphores)},
    _waitStages{std::move(config._waitStages)} {
    Core::debug_check(core);
    createSubmitInfos();
}

void Stage::createCmdPool() {
    _cmdPool = std::make_unique<CmdPool>(_core, CmdPool::Config::Default(_queue->familyIndex(), constants::FRAMES_IN_FLIGHT, 0));
}
void Stage::createCmdBuffers() {
    CTH_CRITICAL(_cmdPool != nullptr, "CmdPool required") {}

    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; ++i)
        _cmdBuffers.emplace_back(_cmdPool.get());
}

void Stage::createSubmitInfos() {
    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; ++i) {
        _submitInfos.emplace_back(
            std::vector{&std::as_const(_cmdBuffers[i])},
            std::vector{std::from_range, _waitStages | std::views::drop(i) | std::views::stride(constants::FRAMES_IN_FLIGHT)},
            std::vector{std::from_range, _signalSemaphores | std::views::drop(i) | std::views::stride(constants::FRAMES_IN_FLIGHT)},
            nullptr
            );
    }
}


PrimaryCmdBuffer& Stage::cmdBuffer() { return current(_cmdBuffers); }
SubmitInfo& Stage::submitInfo() { return current(_submitInfos); }
bool Stage::recording() const { return current(_cmdBuffers).recording(); }


}


namespace cth::vk {
Stage::Config::Config(cth::not_null<Queue const*> queue,
    std::unique_ptr<RenderPass> render_pass, std::span<Semaphore* const> signal_groups,
    std::span<PipelineWaitStage> wait_groups) : _queue{queue}, _renderPass{std::move(render_pass)} {

    addSignalGroups(signal_groups);
    addWaitGroups(wait_groups);
}

Stage::Config& Stage::Config::addSignalGroup(this Config& self, std::span<Semaphore* const> signal_group) {
    CTH_CRITICAL(signal_group.size() == Config::GROUP_SIZE, "signal_group size ({0}) == Config::GROUP_SIZE ({1}) required",
        signal_group.size(), Config::GROUP_SIZE){}

    for(auto const semaphore : signal_group) Semaphore::debug_check(semaphore);

    self._signalSemaphores.append_range(signal_group);
    return self;
}
Stage::Config& Stage::Config::addWaitGroup(this Config& self, std::span<PipelineWaitStage const> wait_group) {
    CTH_CRITICAL(wait_group.size() == Config::GROUP_SIZE, "signal_group size ({0}) == Config::GROUP_SIZE ({1}) required",
        wait_group.size(), Config::GROUP_SIZE){}

    for(auto const& waitStage : wait_group) PipelineWaitStage::debug_check(waitStage);

    self._waitStages.append_range(wait_group);
    return self;
}
Stage::Config& Stage::Config::addSignalGroups(this Config& self, std::span<Semaphore* const> signal_groups) {
    CTH_CRITICAL(signal_groups.size() % GROUP_SIZE == 0, "signal_groups.size() ({0}) % Config::GroupSize ({1}) == 0 required", signal_groups.size(),
        Config::GROUP_SIZE) {}
    auto const view = signal_groups | std::views::chunk(GROUP_SIZE);


    for(auto const signalGroup : view) self.addSignalGroup(signalGroup);
    return self;
}
Stage::Config& Stage::Config::addWaitGroups(this Config& self, std::span<PipelineWaitStage> wait_groups) {
    CTH_CRITICAL(wait_groups.size() % GROUP_SIZE == 0, "signal_groups.size() ({0}) % Config::GroupSize ({1}) == 0 required", wait_groups.size(),
        Config::GROUP_SIZE) {}
    auto const view = wait_groups | std::views::chunk(GROUP_SIZE);


    for(auto const waitGroup : view) self.addWaitGroup(waitGroup);
    return self;
}



}
