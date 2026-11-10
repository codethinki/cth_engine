#include "jvk/graph/stage.hpp"

#include "jvk/base/core.hpp"
#include "jvk/graph/stage_graph.hpp"

#include "cvk/render/sync/semaphore.hpp"

#include <cth/io/log.hpp>

#include <utility>


namespace jvk::dev {
StageSlot::StageSlot(StageGraph& graph, size_t stageId, size_t slot, Queue const& queue, size_t secondaries) :
    graph{graph},
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

PrimaryCmdBuffer& Stage::beginPrimary() {
    CTH_CRITICAL(_status != Status::UNSUBMITTED, "the stage must be unsubmitted") {}

    auto& primary = _stageSlot->primary;
    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    return primary;
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
}
