#include "jvk/graph/stage_graph.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/coro/executor.hpp>
#include <cth/coro/tasks/sync_task.hpp>
#include <cth/io/log.hpp>

#include <algorithm>
#include <exception>
#include <mutex>
#include <ranges>
#include <utility>
#include <vector>


namespace jvk {
using std::vector;

namespace {
    /**
     * @return `1 +` the largest `StageWait::frameOffset` of the config
     */
    [[nodiscard]] uint64_t first_frame_number(StageGraphConfig const& config) {
        uint64_t maxOffset = 0;
        for(auto const& stage : config.stages)
            for(auto const& wait : stage.waits)
                maxOffset = std::max<uint64_t>(maxOffset, wait.frameOffset);
        return maxOffset + 1;
    }

    /**
     * runs the job on the workers of the core once started
     * @return the job, ends when the job ends
     */
    [[nodiscard]] cth::co::sync_task<void> spawn_job(cth::co::executor workers, cth::co::executor_task<void> job) {
        co_await workers.spawn(std::move(job));
    }
}

StageGraph::StageGraph(Core& core, Config config) :
    _core{core},
    _config{std::move(config)},
    _firstFrameNumber{first_frame_number(_config)},
    _frameNumber{_firstFrameNumber - 1} {
    Core::debug_check(core);
    Config::debug_check(_config);

    auto const frameSlots = _config.frameSlots;
    auto const& stages = _config.stages;

    for(size_t slot = 0; slot < frameSlots; ++slot) {
        auto& frameSlot = _frameSlots.emplace_back(*this, slot);
        for(size_t stageId = 0; stageId < stages.size(); ++stageId)
            frameSlot.stageSlots.emplace_back(
                *this,
                stageId,
                slot,
                core.queue(stages[stageId].queueIdx),
                stages[stageId].secondaries
            );
    }

    // the waits and the stage of the frame slot before, once all slots exist
    for(size_t slot = 0; slot < frameSlots; ++slot)
        for(size_t stageId = 0; stageId < stages.size(); ++stageId) {
            auto& stageSlot = _frameSlots[slot].stageSlots[stageId];
            stageSlot.previous = _frameSlots[(slot + frameSlots - 1) % frameSlots].stageSlots[stageId];

            auto& info = stageSlot.submitInfo;
            vector<SemaphoreSubmitInfo> otherQueues{};
            for(auto const& wait : stages[stageId].waits) {
                auto const waitedSlot = (slot + frameSlots - wait.frameOffset % frameSlots) % frameSlots;
                auto const& waited = _frameSlots[waitedSlot].stageSlots[wait.stageId];

                // the first frame of the slot waits for the frame frameOffset frames before it
                SemaphoreSubmitInfo const semaphoreWait{
                    .semaphore = waited.semaphore,
                    .stages = wait.stages,
                    .value = _firstFrameNumber + slot - wait.frameOffset,
                };
                if(&*waited.queue == &*stageSlot.queue) info.waits.push_back(semaphoreWait);
                else otherQueues.push_back(semaphoreWait);
            }
            info.queueWaits = info.waits.size();
            info.waits.append_range(otherQueues);
        }
}

StageGraph::~StageGraph() {
    for(auto const& frameSlot : _frameSlots)
        for(auto const& stage : frameSlot.stages)
            CTH_CRITICAL(
                stage.status() == Stage::Status::UNSUBMITTED,
                "every stage must be submitted or skipped, got stage {} of frame {}",
                stage.id(),
                stage.frameNumber()
            ) {}

    for(auto& frameSlot : _frameSlots)
        for(auto& stageSlot : frameSlot.stageSlots) {
            if(stageSlot.jobOpt.has_value()) stageSlot.jobOpt->wait();

            // a destructor does not throw: a failed wait leaves the gpu work, the destruction goes on
            try {
                cvk::TimelineSemaphore const& semaphore = stageSlot.semaphore;
                semaphore.wait(stageSlot.semaphore.submittedValue());
            }
            catch(cvk::vk_result_exception const& e) {
                JVK_STABLE_ERR(
                    true,
                    "failed to wait for stage {} in ~StageGraph(), got {}",
                    stageSlot.stageId,
                    e.vkResult()
                ) {}
            }
        }
}

cth::co::executor_task<Frame&> StageGraph::nextFrame() {
    auto const number = ++_frameNumber;
    auto& frameSlot = _frameSlots[(number - _firstFrameNumber) % _config.frameSlots];

    if(frameSlot.frameOpt.has_value()) {
        for(auto& stage : frameSlot.stages) {
            if(stage.status() != Stage::Status::UNSUBMITTED) continue;

            bool const waited = std::ranges::any_of(_config.stages, [&stage](StageConfig const& config) {
                return std::ranges::any_of(config.waits, [&stage](StageWait const& wait) {
                    return wait.stageId == stage.id();
                });
            });
            CTH_WARN(
                waited,
                "stage {} of frame {} has no submit or skip, nextFrame() skips it, its waiters waited until now",
                stage.id(),
                stage.frameNumber()
            ) {}

            skip(stage);
        }

        for(auto const& stage : frameSlot.stages)
            co_await stage.submitted();

        // before the gpu wait: the submit of a failed job signals nothing
        rethrowError();

        for(auto const& stage : frameSlot.stages)
            co_await stage;

        //TODO review a short block: the job ends right after its mark, the gpu finished it
        for(auto& stageSlot : frameSlot.stageSlots) {
            if(stageSlot.jobOpt.has_value()) stageSlot.jobOpt->wait();
            stageSlot.jobOpt.reset();
        }
    }

    rethrowError();

    frameSlot.frameOpt.reset();
    frameSlot.stages.clear();
    for(auto& stageSlot : frameSlot.stageSlots)
        frameSlot.stages.emplace_back(stageSlot, number);
    frameSlot.frameOpt.emplace(frameSlot, number);

    co_return *frameSlot.frameOpt;
}

void StageGraph::submit(Stage& stage) {
    auto& stageSlot = *stage._stageSlot;
    CTH_CRITICAL(&*stageSlot.graph != this, "the stage must be of this graph") {}
    CTH_CRITICAL(stage._status != Stage::Status::UNSUBMITTED, "the stage must be unsubmitted") {}
    CTH_CRITICAL(stageSlot.primary.rendering() != nullptr, "the primary must have no open rendering") {}
    CTH_CRITICAL(stageSlot.jobOpt.has_value(), "the job of the last frame of the slot must be done") {}

    stageSlot.primary.optEnd();
    stage._status = Stage::Status::SUBMITTED;

    stageSlot.jobOpt.emplace(spawn_job(
        core().executor(),
        submitJob(
            stageSlot,
            stage._frameNumber,
            std::move(stage._extraWaits),
            std::move(stage._extraBinaryWaits),
            Stage::Status::SUBMITTED
        )
    ));
    stageSlot.jobOpt->start();
}

void StageGraph::skip(Stage& stage) {
    auto& stageSlot = *stage._stageSlot;
    CTH_CRITICAL(&*stageSlot.graph != this, "the stage must be of this graph") {}
    CTH_CRITICAL(stage._status != Stage::Status::UNSUBMITTED, "the stage must be unsubmitted") {}
    CTH_CRITICAL(stageSlot.primary.recording(), "a skipped stage must not begin its primary") {}
    CTH_CRITICAL(stageSlot.jobOpt.has_value(), "the job of the last frame of the slot must be done") {}

    stage._status = Stage::Status::SKIPPED;

    stageSlot.jobOpt.emplace(spawn_job(
        core().executor(),
        submitJob(
            stageSlot,
            stage._frameNumber,
            std::move(stage._extraWaits),
            std::move(stage._extraBinaryWaits),
            Stage::Status::SKIPPED
        )
    ));
    stageSlot.jobOpt->start();
}

cth::co::executor_task<void> StageGraph::submitJob(
    dev::StageSlot& stageSlot,
    uint64_t frameNumber,
    vector<SemaphoreSubmitInfo> extraWaits,
    semaphore_waits_t extraBinaryWaits,
    Stage::Status status
) {
    auto const& info = stageSlot.submitInfo;

    try {
        // off the strand: the submits on one queue follow the order of the waits, a wait-before-signal would block
        co_await stageSlot.previous->semaphore.submitted(frameNumber - 1);
        for(auto const& wait : info.waits | std::views::take(info.queueWaits))
            co_await wait.semaphore->submitted(wait.value);
        for(auto const& wait : extraWaits)
            co_await wait.semaphore->submitted(wait.value);

        //TODO 3b: under _statesMutex append() the states of the primary: the prologue barriers and the releases
        //TODO 3b: release() on the last queues of another family, the submit waits for the returned values

        co_await stageSlot.queue->executor().schedule();

        cvk::SubmitInfo submitInfo{.signals = {info.signal}};
        if(status == Stage::Status::SUBMITTED) {
            //TODO 3b: the prologue records the acquire halves and the barriers of the first uses
            stageSlot.prologue.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            stageSlot.prologue.end();
            for(PrimaryCmdBuffer const& cmdBuffer : info.cmdBuffers)
                submitInfo.cmdBuffers.emplace_back(cmdBuffer);
        }

        submitInfo.waits.reserve(info.waits.size() + extraWaits.size() + extraBinaryWaits.size());
        submitInfo.waits.append_range(info.waits);
        submitInfo.waits.append_range(extraWaits);
        submitInfo.waits.append_range(extraBinaryWaits);

        stageSlot.queue->vk().submit(submitInfo);
    }
    catch(...) {
        storeError(std::current_exception());
    }

    // also after an error: the waiters of the mark go on, nextFrame() rethrows the error
    stageSlot.submitInfo.next(_config.frameSlots);
    stageSlot.semaphore.markSubmitted(frameNumber);

    //TODO 3b: after the gpu finished the submit: clear the stasis
}

void StageGraph::rethrowError() {
    std::scoped_lock const lock{_errorMutex};
    if(_errorPtr) std::rethrow_exception(_errorPtr);
}

void StageGraph::storeError(std::exception_ptr errorPtr) noexcept {
    std::scoped_lock const lock{_errorMutex};
    if(!_errorPtr) _errorPtr = std::move(errorPtr);
}
}
