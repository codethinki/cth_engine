#include "RenderStage.hpp"

#include <utility>

#include "src/vulkan/base/queue/CthQueue.hpp"
#include "src/vulkan/base/queue/CthSubmitInfo.hpp"
#include "src/vulkan/render/cmd/CthCmdBuffer.hpp"
#include "src/vulkan/render/cmd/CthCmdPool.hpp"
#include "src/vulkan/render/control/CthFence.hpp"

namespace cth::vk {

RenderStage::RenderStage(Core const& core, Config config): _core{&core}, _config{std::move(config)} {
    init();
}
RenderStage::RenderStage(Core const& core, Config config, create_t) : RenderStage{core, std::move(config)} {
    create();
}
RenderStage::~RenderStage() { optDestroy(); }
void RenderStage::create() {
    optDestroy();

    createFences();
    createCmdBuffers();
    createSubmitInfos();
}
void RenderStage::destroy() {
    _submitInfos.clear();
    for(auto& buffer : _primaryCmdBuffers) buffer.destroy();
    for(auto& buffer : _secondaryCmdBuffers) buffer.destroy();
    for(auto& pool : _cmdPools) pool.destroy();
}
RenderStageCmdBuffers RenderStage::begin() {
    auto& primary = primaryCmdBuffer();
    primary.begin();
    return RenderStageCmdBuffers{
        .cmdBuffer = &primary,
        .secondaryCmdBuffers = secondaryCmdBuffers(),
    };
}
void RenderStage::end() {
    primaryCmdBuffer().end();
}
void RenderStage::submit() {
    optEnd();
    queue().submit(submitInfo());

    next();
}
void RenderStage::skip() {
    queue().skip(submitInfo());
    next();
}
VkResult RenderStage::wait(size_t timeout) const { return fence().wait(timeout); }
void RenderStage::wait() const { fence().wait(); }

void RenderStage::initFences() { for(size_t i = 0; i < GROUP_SIZE; i++) _fences.emplace_back(*_core); }
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
void RenderStage::initSubmitInfos() { _submitInfos.reserve(GROUP_SIZE); }
void RenderStage::init() {
    initFences();
    initCmdPools();
    initCmdBuffers();
    initSubmitInfos();
}
void RenderStage::createFences() {
    for(auto& fence : _fences) fence.create(VK_FENCE_CREATE_SIGNALED_BIT);
}
void RenderStage::createCmdPools() {
    for(auto& pool : _cmdPools) pool.create();
}


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
        for(ptrdiff_t i = 0; std::cmp_less(i, GROUP_SIZE); i++)
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
        std::vector signalSemaphores{std::from_range, _config.signalSemaphores | views::drop_stride(i, GROUP_SIZE)};
        std::vector waitStages{std::from_range, _config.waitStages | views::drop_stride(i, GROUP_SIZE)};

        _submitInfos.emplace_back(primaryCmdBuffers, waitStages, signalSemaphores, &_fences[i]);
    }
}
size_t RenderStage::secondaryChunkSize() const { return _secondaryCmdBuffers.size() / GROUP_SIZE; }

PrimaryCmdBuffer& RenderStage::primaryCmdBuffer() { return _primaryCmdBuffers[_subIndex]; }
PrimaryCmdBuffer const& RenderStage::primaryCmdBuffer() const {
    return _primaryCmdBuffers[_subIndex];
}

std::vector<SecondaryCmdBuffer*> RenderStage::secondaryCmdBuffers() {
    auto chunks = _secondaryCmdBuffers | cth::views::split_into(GROUP_SIZE);
    return {std::from_range, chunks[static_cast<ptrdiff_t>(_subIndex)] | cth::views::to_ptr_range};
}
SubmitInfo& RenderStage::submitInfo() { return _submitInfos[_subIndex]; }
Fence const& RenderStage::fence() const { return _fences[_subIndex]; }
void RenderStage::next() { ++_subIndex %= GROUP_SIZE; }
bool RenderStage::created() const {
    return _cmdPools[0].created();
}
bool RenderStage::recording() const {
    return primaryCmdBuffer().recording();
}

}
