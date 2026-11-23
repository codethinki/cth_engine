#include "jvk/graph/stage.hpp"

#include "jvk/base/core.hpp"
#include "jvk/graph/stage_graph.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/data/descriptor/resource_heap.hpp"
#include "cvk/data/descriptor/sampler_heap.hpp"
#include "cvk/render/sync/semaphore.hpp"

#include <cth/io/log.hpp>

#include <exception>
#include <span>
#include <utility>
#include <vector>


namespace jvk::dev {
namespace {
    /**
     * @return the heaps of the core, none on a family without graphics and compute
     */
    [[nodiscard]] cvk::HeapInheritance stage_heaps(Core const& core, Queue const& queue) {
        cvk::Core const& vk = core;
        auto const* familyPtr = vk.physicalDevice().queueFamily(queue.vk().familyIndex());
        bool const binds = familyPtr != nullptr
            && (familyPtr->vkProperties.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
            != 0;
        if(!binds)
            return {};

        auto const& heaps = core.config().heaps;
        return cvk::HeapInheritance{
            .resourceHeapPtr = heaps.createsResourceHeap() ? &core.resourceHeap().vk() : nullptr,
            .samplerHeapPtr = heaps.createsSamplerHeap() ? &core.samplerHeap().vk() : nullptr,
        };
    }
}

void SlotSubmit::skipInfo(
    SubmitInfo& info,
    std::span<SemaphoreSubmitInfo const> extraWaits,
    std::span<cvk::SemaphoreSubmitInfo const> extraBinaryWaits
) const {
    info.clear();
    info.waits.reserve(waits.size() + extraWaits.size());
    info.waits.append_range(waits);
    info.waits.append_range(extraWaits);
    info.binaryWaits.append_range(extraBinaryWaits);
    info.signals.push_back(signal);
}

void SlotSubmit::addCmdBuffers(SubmitInfo& info, bool prologue, bool epilogue) const {
    // the order of SlotSubmit::cmdBuffers: the prologue, the primary, the epilogue
    if(prologue)
        info.cmdBuffers.push_back(cmdBuffers[0]);
    info.cmdBuffers.push_back(cmdBuffers[1]);
    if(epilogue)
        info.cmdBuffers.push_back(cmdBuffers[2]);
}

StageSlot::StageSlot(
    StageGraph& graph,
    cth::co::catching_scope& jobs,
    size_t stageId,
    size_t slot,
    Queue const& queue,
    size_t secondaries
) :
    graph{graph},
    jobs{jobs},
    stageId{stageId},
    slot{slot},
    queue{queue},
    heaps{stage_heaps(graph.core(), queue)},
    semaphore{graph.core(), cvk::create, graph.firstFrameNumber() - 1},
    slotSubmit{.signal = {.semaphore = semaphore, .value = graph.firstFrameNumber() + slot}},
    submitDone{graph.firstFrameNumber() - 1},
    previous{*this},
    prologue{graph.core(), queue.vk().familyIndex(), cvk::create},
    primary{graph.core(), queue.vk().familyIndex(), cvk::create},
    epilogue{graph.core(), queue.vk().familyIndex(), cvk::create},
    stasis{graph.core()} {
    slotSubmit.cmdBuffers = {prologue, primary, epilogue};

    this->secondaries.reserve(secondaries);
    for(size_t i = 0; i < secondaries; ++i)
        this->secondaries.emplace_back(graph.core(), queue.vk().familyIndex(), cvk::create);
}
}


namespace jvk {
Stage::Stage(dev::StageSlot& stageSlot, uint64_t frameNumber) : _stageSlot{stageSlot},
    _frameNumber{frameNumber} {}

scoped_submit Stage::begin() {
    CTH_CRITICAL(_status != Status::UNSUBMITTED, "the stage must be unsubmitted") {}

    _stageSlot->primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    cvk::PrimaryCmdBuffer const& vk = _stageSlot->primary;
    auto const [resourceHeapPtr, samplerHeapPtr] = _stageSlot->heaps;
    if(resourceHeapPtr != nullptr)
        vk.bind(*resourceHeapPtr);
    if(samplerHeapPtr != nullptr)
        vk.bind(*samplerHeapPtr);

    return scoped_submit{*this, _stageSlot->jobs};
}

SecondaryCmdBuffer& Stage::beginSecondary(
    size_t secondaryIdx,
    std::optional<Inheritance> const& inheritanceOpt
) {
    auto& secondary = this->secondary(secondaryIdx);
    if(inheritanceOpt.has_value())
        secondary.begin(*inheritanceOpt, 0, _stageSlot->heaps);
    else
        secondary.begin(0, _stageSlot->heaps);
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
    return SecondaryCmdBuffer{_stageSlot->graph->core(), queue().vk().familyIndex(), cvk::create};
}

void Stage::await(SemaphoreSubmitInfo const& wait) {
    CTH_CRITICAL(_status != Status::UNSUBMITTED, "the stage must be unsubmitted") {}
    _stageSlot->extraWaits.push_back(wait);
}

void Stage::await(cvk::Semaphore const& binary, VkPipelineStageFlags2 stages) {
    CTH_CRITICAL(_status != Status::UNSUBMITTED, "the stage must be unsubmitted") {}
    _stageSlot->extraBinaryWaits.emplace_back(binary, stages);
}

void Stage::present(dev::PendingPresent present) {
    CTH_CRITICAL(_status != Status::UNSUBMITTED, "the stage must be unsubmitted") {}
    _stageSlot->presents.push_back(std::move(present));
}

void Stage::submit() { _stageSlot->graph->submit(*this); }

void Stage::skip() { _stageSlot->graph->skip(*this); }

scoped_submit::~scoped_submit() {
    // an exception: the next nextFrame() skips the stage
    if(!_stagePtr || std::uncaught_exceptions() > _uncaughtExceptions)
        return;
    if(_stagePtr->status() != Stage::Status::UNSUBMITTED)
        return;

    // a destructor is no task: it stores the failure itself
    try { _stagePtr->submit(); }
    catch(...) { _jobs->store(std::current_exception()); }
}
}
