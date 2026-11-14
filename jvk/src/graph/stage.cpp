#include "jvk/graph/stage.hpp"

#include "jvk/base/core.hpp"
#include "jvk/graph/stage_graph.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/render/sync/semaphore.hpp"

#include <cth/io/log.hpp>

#include <exception>
#include <span>
#include <utility>
#include <vector>


namespace jvk::dev {
namespace {
    /**
     * the waits of the submit info, then the extra waits
     */
    [[nodiscard]] std::vector<cvk::SemaphoreSubmitInfo> all_waits(
        SubmitInfo const& info,
        std::span<SemaphoreSubmitInfo const> extraWaits,
        std::span<cvk::SemaphoreSubmitInfo const> extraBinaryWaits
    ) {
        std::vector<cvk::SemaphoreSubmitInfo> waits{};
        waits.reserve(info.waits.size() + extraWaits.size() + extraBinaryWaits.size());
        waits.append_range(info.waits);
        waits.append_range(extraWaits);
        waits.append_range(extraBinaryWaits);
        return waits;
    }
}

void SubmitInfo::submit(
    Queue const& queue,
    std::span<SemaphoreSubmitInfo const> extraWaits,
    std::span<cvk::SemaphoreSubmitInfo const> extraBinaryWaits
) const {
    cvk::SubmitInfo submitInfo{.waits = all_waits(*this, extraWaits, extraBinaryWaits), .signals = {signal}};
    submitInfo.cmdBuffers.reserve(cmdBuffers.size());
    for(PrimaryCmdBuffer const& cmdBuffer : cmdBuffers)
        submitInfo.cmdBuffers.emplace_back(cmdBuffer);

    queue.vk().submit(submitInfo);
}

void SubmitInfo::skip(
    Queue const& queue,
    std::span<SemaphoreSubmitInfo const> extraWaits,
    std::span<cvk::SemaphoreSubmitInfo const> extraBinaryWaits
) const {
    queue.vk().submit({.waits = all_waits(*this, extraWaits, extraBinaryWaits), .signals = {signal}});
}

StageSlot::StageSlot(
    StageGraph& graph,
    first_error& error,
    size_t stageId,
    size_t slot,
    Queue const& queue,
    size_t secondaries
) :
    graph{graph},
    error{error},
    stageId{stageId},
    slot{slot},
    queue{queue},
    semaphore{graph.core(), cvk::create, graph.firstFrameNumber() - 1},
    submitInfo{.signal = {.semaphore = semaphore, .value = graph.firstFrameNumber() + slot}},
    previous{*this},
    prologue{graph.core(), queue.vk().familyIndex()},
    primary{graph.core(), queue.vk().familyIndex()},
    stasis{graph.core()} {
    submitInfo.cmdBuffers = {prologue, primary};

    this->secondaries.reserve(secondaries);
    for(size_t i = 0; i < secondaries; ++i)
        this->secondaries.emplace_back(graph.core(), queue.vk().familyIndex());
}
}


namespace jvk {
Stage::Stage(dev::StageSlot& stageSlot, uint64_t frameNumber) : _stageSlot{stageSlot}, _frameNumber{frameNumber} {}

Stage::PrimaryRecording Stage::beginPrimary() {
    CTH_CRITICAL(_status != Status::UNSUBMITTED, "the stage must be unsubmitted") {}

    _stageSlot->primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    return PrimaryRecording{*this};
}

SecondaryCmdBuffer& Stage::beginSecondary(size_t secondaryIdx, std::optional<Inheritance> const& inheritanceOpt) {
    auto& secondary = this->secondary(secondaryIdx);
    if(inheritanceOpt.has_value()) secondary.begin(*inheritanceOpt);
    else secondary.begin();
    return secondary;
}

SecondaryCmdBuffer& Stage::secondary(size_t secondaryIdx) {
    auto& secondaries = _stageSlot->secondaries;
    CTH_CRITICAL(
        secondaryIdx >= secondaries.size(),
        "secondary index must be < {}, got {}",
        secondaries.size(),
        secondaryIdx
    ) {}
    return secondaries[secondaryIdx];
}

SecondaryCmdBuffer Stage::persistentSecondary() const {
    return SecondaryCmdBuffer{_stageSlot->graph->core(), queue().vk().familyIndex()};
}

void Stage::await(SemaphoreSubmitInfo const& wait) {
    CTH_CRITICAL(_status != Status::UNSUBMITTED, "the stage must be unsubmitted") {}
    _extraWaits.push_back(wait);
}

void Stage::await(cvk::Semaphore const& binary, VkPipelineStageFlags2 stages) {
    CTH_CRITICAL(_status != Status::UNSUBMITTED, "the stage must be unsubmitted") {}
    _extraBinaryWaits.emplace_back(binary, stages);
}

void Stage::submit() { _stageSlot->graph->submit(*this); }

void Stage::skip() { _stageSlot->graph->skip(*this); }

Stage::PrimaryRecording::~PrimaryRecording() {
    // an exception: nextFrame() skips the stage
    if(!_stagePtr || std::uncaught_exceptions() > _uncaughtExceptions) return;
    if(_stagePtr->status() != Status::UNSUBMITTED) return;

    try {
        _stagePtr->submit();
    }
    catch(...) {
        _stagePtr->_stageSlot->error->store(std::current_exception());
    }
}
}
