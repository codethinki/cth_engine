#include "RenderStage.hpp"

#include "src/vulkan/base/queue/CthQueue.hpp"
#include "src/vulkan/base/queue/CthSubmitInfo.hpp"
#include "src/vulkan/render/cmd/CthCmdBuffer.hpp"
#include "src/vulkan/render/cmd/CthCmdPool.hpp"
#include "src/vulkan/render/control/CthFence.hpp"
#include "src/vulkan/render/control/CthPipelineWaitStage.hpp"

namespace cth::vk {

RenderStage::RenderStage(Core const& core, Config config): _core{&core}, _config{std::move(config)} { initCmdBuffers(); }
RenderStage::RenderStage(Core const& core, Config config, create_t) : RenderStage{core, std::move(config)} {}
RenderStage::~RenderStage() { optDestroy(); }
void RenderStage::create() {
    optDestroy();

    
    createCmdBuffers();
    createSubmitInfos();
}
RenderStageCmdBuffers RenderStage::begin() {
    auto& primary = primaryCmdBuffer();
    primary.begin();
    return RenderStageCmdBuffers{
        .cmdBuffer = &primary,
        .secondaryCmdBuffers = secondaryCmdBuffers(),
    };
}
void RenderStage::submit() {
    optEnd();
    queue().submit(submitInfo());

    ++_subIndex;
}
void RenderStage::skip() {
    queue().skip(submitInfo());
    ++_subIndex;
}
VkResult RenderStage::wait(size_t timeout) const {
    return fence().wait(timeout);
}
void RenderStage::wait() const {
    fence().wait();
}


void RenderStage::initCmdPools() {
    uint32_t const subStages = _config.subStages;

    bool const parallelFramesInFlight = _config.parallelFrameInFlightRecording();
    bool const parallelSubStages = _config.parallelSubStageRecording();

    uint32_t const buffersPerPool = parallelFramesInFlight ? 1 : GROUP_SIZE;

    uint32_t const primaryPools = (GROUP_SIZE + 1) - buffersPerPool;

    uint32_t const nonParallelSecondaries = parallelSubStages ? 0 : subStages;
    uint32_t const secondaryOnlyPools = subStages - nonParallelSecondaries;

    uint32_t const secondaryBuffers = nonParallelSecondaries * buffersPerPool;

    for(uint32_t i = 0; i < primaryPools; i++) {
        _cmdPools.emplace_back(*_core, CmdPool::Config::Default(*_config.queue, buffersPerPool, secondaryBuffers));

        for(uint32_t j = 0; j < secondaryOnlyPools; j++)
            _cmdPools.emplace_back(*_core, CmdPool::Config::Default(*_config.queue, 0, buffersPerPool));
    }
}
void RenderStage::initCmdBuffers() { _primaryCmdBuffers.resize(GROUP_SIZE); }
void RenderStage::initFences() { for(size_t i = 0; i < GROUP_SIZE; i++) _fences.emplace_back(*_core); }
void RenderStage::initSubmitInfos() { _submitInfos.reserve(GROUP_SIZE); }
void RenderStage::createPrimaryCmdBuffers() {
    for(size_t i = 0; i < GROUP_SIZE; i++) {
        auto& pool = _cmdPools[_cmdPools.size() / GROUP_SIZE * i];

        _primaryCmdBuffers[i].create(pool);
    }
}
void RenderStage::createSecondaryCmdBuffers() {
    if(_cmdPools.size() == 1) {
        for(auto& buffer : _secondaryCmdBuffers) buffer.create(_cmdPools.front());
        return;
    }

    auto const chunks = _secondaryCmdBuffers | cth::views::split_into(GROUP_SIZE);

    if(_cmdPools.size() == GROUP_SIZE) {
        for(ptrdiff_t i = 0; i < GROUP_SIZE; i++)
            for(auto& buffer : chunks[i]) buffer.create(_cmdPools[i]);

        return;
    }
    auto const poolChunkSize = _cmdPools.size() / GROUP_SIZE;
    size_t const subStages = _config.subStages;

    for(size_t i = 0; i < GROUP_SIZE; i++)
        for(size_t j = 0; j < subStages; j++)
            _secondaryCmdBuffers[i * subStages + j].create(_cmdPools[i * poolChunkSize + j]);

}
void RenderStage::createCmdBuffers() {
    createPrimaryCmdBuffers();
    createSecondaryCmdBuffers();
}
void RenderStage::createSubmitInfos() {
    for(size_t i = 0; i < GROUP_SIZE; i++) {
        std::vector primaryCmdBuffers{&_primaryCmdBuffers[i]};
        std::vector signalSemaphores{std::from_range, _config.signalSemaphores | std::views::drop(i) | std::views::stride(GROUP_SIZE)};
        std::vector waitStages{std::from_range, _config.waitStages | std::views::drop(i) | std::views::stride(GROUP_SIZE)};

        _submitInfos.emplace_back(primaryCmdBuffers, waitStages, signalSemaphores, &_fences[i]);
    }
}
size_t RenderStage::secondaryChunkSize() const { return _secondaryCmdBuffers.size() / GROUP_SIZE; }

PrimaryCmdBuffer& RenderStage::primaryCmdBuffer() { return _primaryCmdBuffers[_subIndex]; }

std::vector<SecondaryCmdBuffer*> RenderStage::secondaryCmdBuffers() {
    auto chunks = _secondaryCmdBuffers | cth::views::split_into(GROUP_SIZE);
    return {std::from_range, chunks[_subIndex] | cth::views::to_ptr_range};
}
SubmitInfo& RenderStage::submitInfo() { return _submitInfos[_subIndex]; }
Fence const& RenderStage::fence() const { return _fences[_subIndex]; }

}
