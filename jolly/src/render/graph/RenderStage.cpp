#include "jolly/render/graph/RenderStage.hpp"

#include "jolly/core/core.hpp"
#include "jolly/render/submit/queue.hpp"
#include "jolly/render/submit/submit_info.hpp"
#include "jolly/render/sync/RenderPulse.hpp"
#include "jolly/render/submit/cmd/primary_cmd_buffer.hpp"
#include "jolly/render/submit/cmd/secondary_cmd_buffer.hpp"
#include "jolly/utility/types.hpp"

#include "jvk/base/queue/queue.hpp"
#include "jvk/base/queue/submit_info.hpp"
#include "jvk/render/cmd/cmd_pool.hpp"

#include <cth/algorithm/views.hpp>

#include <utility>


namespace jly {

RenderStage::RenderStage(Core const& core, RenderPulse const& pulse, Config config) : _core{&core},
    _pulse{&pulse},
    _config{std::move(config)} { init(); }

RenderStage::RenderStage(
    Core const& core,
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
    reset();

    queue().skip(submitInfo());

    primaryCmdBuffer().discardTasks();
}

void RenderStage::wait() const { submitInfo().wait(); }


void RenderStage::reset() { submitInfo().reset_fence(); }


void RenderStage::initCmdPools() {
    uint32_t const subStages = _config.subStages;

    bool const parallelFramesInFlight = _config.parallelFrameInFlightRecording();
    bool const parallelSubStages = _config.parallelSubStageRecording();

    uint32_t const buffersPerPool = parallelFramesInFlight ? 1 : framesInFlight();

    uint32_t const primaryPools = (framesInFlight() + 1) - buffersPerPool;

    uint32_t const nonParallelSecondaries = parallelSubStages ? 0 : subStages;
    uint32_t const secondaryOnlyPools = subStages - nonParallelSecondaries;

    uint32_t const secondaryBuffers = nonParallelSecondaries * buffersPerPool;

    for(uint32_t i = 0; i < primaryPools; i++) {
        _cmdPools.emplace_back(
            _core->raw(),
            jvk::CmdPool::Config::Default(_config.queue->raw(), buffersPerPool, secondaryBuffers)
        );

        for(uint32_t j = 0; j < secondaryOnlyPools; j++)
            _cmdPools.emplace_back(_core->raw(), jvk::CmdPool::Config::Default(_config.queue->raw(), 0, buffersPerPool));
    }
}

void RenderStage::initCmdBuffers() {
    auto const primaries = framesInFlight();

    _primaryCmdBuffers.reserve(primaries);

    for(size_t i = 0; i < primaries; i++)
        _primaryCmdBuffers.emplace_back(PrimaryCmdBuffer::Config{});

    auto const secondaries = primaries * _config.subStages;
    _secondaryCmdBuffers.reserve(secondaries);
    for(size_t i = 0; i < secondaries; i++)
        _secondaryCmdBuffers.emplace_back(
            jly::SecondaryCmdBufferConfig{}
        );
}
void RenderStage::initSubmitInfos() { _submitInfos.reserve(framesInFlight()); }

void RenderStage::init() {
    initCmdPools();
    initCmdBuffers();
    initSubmitInfos();
}

void RenderStage::createCmdPools() { for(auto& pool : _cmdPools) pool.create(); }


void RenderStage::createPrimaryCmdBuffers() {
    uint32_t const poolsPerFrame = _cmdPools.size() / framesInFlight();

    for(size_t i = 0; i < framesInFlight(); i++) {
        auto& pool = _cmdPools[i * poolsPerFrame];
        _primaryCmdBuffers[i].create(pool);
    }
}

void RenderStage::createSecondaryCmdBuffers() {
    if(_secondaryCmdBuffers.empty()) return;

    bool const parallelSubStages = _config.parallelSubStageRecording();
    uint32_t const subStages = _config.subStages;
    uint32_t const poolsPerFrame = _cmdPools.size() / framesInFlight();

    for(size_t frameIdx = 0; frameIdx < framesInFlight(); frameIdx++)
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
    for(size_t i = 0; i < framesInFlight(); i++) {
        std::vector primaryCmdBuffers{&_primaryCmdBuffers[i]};
        std::vector signalSemaphores{
            std::from_range,
            _config.signalSemaphores | cth::views::drop_stride(i, framesInFlight())
        };
        std::vector waitStages{std::from_range, _config.waitStages | cth::views::drop_stride(i, framesInFlight())};

        _submitInfos.emplace_back(*_core, primaryCmdBuffers, waitStages, signalSemaphores, true);
    }
}

size_t RenderStage::secondaryChunkSize() const { return _secondaryCmdBuffers.size() / framesInFlight(); }

PrimaryCmdBuffer& RenderStage::primaryCmdBuffer() { return _primaryCmdBuffers[subIndex()]; }
PrimaryCmdBuffer const& RenderStage::primaryCmdBuffer() const { return _primaryCmdBuffers[subIndex()]; }

std::vector<SecondaryCmdBuffer*> RenderStage::secondaryCmdBuffers() {
    if(_secondaryCmdBuffers.empty()) return {};
    auto chunks = _secondaryCmdBuffers | cth::views::split_into(framesInFlight());
    return {std::from_range, chunks[static_cast<ptrdiff_t>(subIndex())] | cth::views::to_ptr_range};
}

SubmitInfo& RenderStage::submitInfo() { return _submitInfos[subIndex()]; }

SubmitInfo const& RenderStage::submitInfo() const { return _submitInfos[subIndex()]; }

size_t RenderStage::subIndex() const { return _pulse->get(); }
size_t RenderStage::framesInFlight() const { return _pulse->framesInFlight(); }
bool RenderStage::created() const { return !_cmdPools.empty() && _cmdPools[0].created(); }
bool RenderStage::recording() const { return primaryCmdBuffer().recording(); }


RenderStage::RenderStage(RenderStage&& other) noexcept = default;

}
