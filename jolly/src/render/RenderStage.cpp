#include "jolly/render/RenderStage.hpp"

#include "jolly/render/RenderPulse.hpp"
#include "jolly/render/cmd/primary_cmd_buffer.hpp"
#include "jolly/render/cmd/secondary_cmd_buffer.hpp"
#include "jolly/utility/types.hpp"

#include "jvk/base/queue/queue.hpp"
#include "jvk/base/queue/submit_info.hpp"
#include "jvk/render/cmd/cmd_pool.hpp"

#include <cth/algorithm/views.hpp>

#ifdef VOID
#error "fuck"
#endif

#include <cth/coro/task.hpp>

#include <utility>


namespace jly {

RenderStage::RenderStage(jvk::Core const& core, RenderPulse const& pulse, Config config) : _core{&core},
    _pulse{&pulse},
    _config{std::move(config)} { init(); }

RenderStage::RenderStage(
    jvk::Core const& core,
    RenderPulse const& pulse,
    Config config,
    create_t
) : RenderStage{
    core,
    pulse,
    std::move(config)
} { create(); }

RenderStage::~RenderStage() { optDestroy(); }

void RenderStage::create() {
    optDestroy();

    createFences();
    createCmdPools();
    createCmdBuffers();
    createSubmitInfos();
}

void RenderStage::destroy() {
    _submitInfos.clear();
    for(auto& buffer : _secondaryCmdBuffers) buffer.destroy();
    for(auto& buffer : _primaryCmdBuffers) buffer.destroy();
    for(auto& pool : _cmdPools) pool.destroy();
}

RenderStageCmdBuffers RenderStage::begin() {
    wait();

    CTH_CRITICAL(recording(), "stage must not be recording to begin") {}

    auto& primary = primaryCmdBuffer();
    primary.begin();
    return RenderStageCmdBuffers{
        .cmdBuffer = &primary,
        .secondaryCmdBuffers = secondaryCmdBuffers(),
    };
}

void RenderStage::end() {
    CTH_CRITICAL(!recording(), "stage must be recording to end") {}
    primaryCmdBuffer().end();
}

void RenderStage::submit() {
    optEnd();
    reset();
    queue().submit(submitInfo());

    
    //TODO extract fence handle and add to boost::asio
}

void RenderStage::skip() {
    CTH_WARN(recording(), "stage should not be recording when skipping a submit") {}

    wait();
    fence().reset();

    queue().skip(submitInfo());

    primaryCmdBuffer().discardTasks();
}

VkResult RenderStage::wait(size_t timeout) const { return fence().wait(timeout); }
void RenderStage::wait() const { fence().wait(); }


void RenderStage::reset() const { fence().reset(); }



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
        _cmdPools.emplace_back(
            *_core,
            jvk::CmdPool::Config::Default(*_config.queue, buffersPerPool, secondaryBuffers)
        );

        for(uint32_t j = 0; j < secondaryOnlyPools; j++)
            _cmdPools.emplace_back(*_core, jvk::CmdPool::Config::Default(*_config.queue, 0, buffersPerPool));
    }
}

void RenderStage::initCmdBuffers() {
    _primaryCmdBuffers.reserve(GROUP_SIZE);

    for(size_t i = 0; i < GROUP_SIZE; i++)
        _primaryCmdBuffers.emplace_back(PrimaryCmdBuffer::Config{});

    auto const secondaries = static_cast<size_t>(GROUP_SIZE) * _config.subStages;
    _secondaryCmdBuffers.reserve(secondaries);
    for(size_t i = 0; i < secondaries; i++)
        _secondaryCmdBuffers.emplace_back(
            jly::SecondaryCmdBufferConfig{}
        );
}
void RenderStage::initSubmitInfos() { _submitInfos.reserve(GROUP_SIZE); }

void RenderStage::init() {
    initFences();
    initCmdPools();
    initCmdBuffers();
    initSubmitInfos();
}

void RenderStage::createFences() { for(auto& fence : _fences) fence.create(VK_FENCE_CREATE_SIGNALED_BIT); }
void RenderStage::createCmdPools() { for(auto& pool : _cmdPools) pool.create(); }


void RenderStage::createPrimaryCmdBuffers() {
    uint32_t const poolsPerFrame = _cmdPools.size() / GROUP_SIZE;

    for(size_t i = 0; i < GROUP_SIZE; i++) {
        auto& pool = _cmdPools[i * poolsPerFrame];
        _primaryCmdBuffers[i].create(pool);
    }
}

void RenderStage::createSecondaryCmdBuffers() {
    if(_secondaryCmdBuffers.empty()) return;

    bool const parallelSubStages = _config.parallelSubStageRecording();
    uint32_t const subStages = _config.subStages;
    uint32_t const poolsPerFrame = _cmdPools.size() / GROUP_SIZE;

    for(size_t frameIdx = 0; frameIdx < GROUP_SIZE; frameIdx++)
        for(size_t subStageIdx = 0; subStageIdx < subStages; subStageIdx++) {
            auto const bufferIdx = frameIdx * subStages + subStageIdx;

            size_t poolIdx;
            if(parallelSubStages)
                poolIdx = frameIdx * poolsPerFrame + 1 + subStageIdx;
            else
                poolIdx = frameIdx * poolsPerFrame;

            _secondaryCmdBuffers[bufferIdx].create(_cmdPools[poolIdx]);
        }
}

void RenderStage::createCmdBuffers() {
    createPrimaryCmdBuffers();
    createSecondaryCmdBuffers();
}

void RenderStage::createSubmitInfos() {
    for(size_t i = 0; i < GROUP_SIZE; i++) {
        std::vector primaryCmdBuffers{&_primaryCmdBuffers[i].raw()};
        std::vector signalSemaphores{
            std::from_range,
            _config.signalSemaphores | cth::views::drop_stride(i, GROUP_SIZE)
        };
        std::vector waitStages{std::from_range, _config.waitStages | cth::views::drop_stride(i, GROUP_SIZE)};

        _submitInfos.emplace_back(primaryCmdBuffers, waitStages, signalSemaphores, &_fences[i]);
    }
}

size_t RenderStage::secondaryChunkSize() const { return _secondaryCmdBuffers.size() / GROUP_SIZE; }

PrimaryCmdBuffer& RenderStage::primaryCmdBuffer() { return _primaryCmdBuffers[subIndex()]; }
PrimaryCmdBuffer const& RenderStage::primaryCmdBuffer() const { return _primaryCmdBuffers[subIndex()]; }

std::vector<SecondaryCmdBuffer*> RenderStage::secondaryCmdBuffers() {
    if(_secondaryCmdBuffers.empty()) return {};
    auto chunks = _secondaryCmdBuffers | cth::views::split_into(GROUP_SIZE);
    return {std::from_range, chunks[static_cast<ptrdiff_t>(subIndex())] | cth::views::to_ptr_range};
}

jvk::SubmitInfo& RenderStage::submitInfo() { return _submitInfos[subIndex()]; }
jvk::Fence const& RenderStage::fence() const { return _fences[subIndex()]; }
size_t RenderStage::subIndex() const { return _pulse->get(); }
bool RenderStage::created() const { return !_cmdPools.empty() && _cmdPools[0].created(); }
bool RenderStage::recording() const { return primaryCmdBuffer().recording(); }


RenderStage::RenderStage(RenderStage&& other) noexcept = default;

}
