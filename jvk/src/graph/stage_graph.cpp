#include "jvk/graph/stage_graph.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/sync/family_transfer.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/coro/all.hpp>
#include <cth/coro/executor.hpp>
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
     * transition of the first use from `VK_IMAGE_LAYOUT_UNDEFINED`, no queue family ownership transfer
     * @details `src` waits for the stages of the first use, no access: like `cvk::layout_transition()`
     */
    [[nodiscard]] constexpr cvk::ImageBarrierInfo from_undefined(
        ImageState const& first,
        uint32_t mipLevel
    ) {
        return cvk::ImageBarrierInfo{
            .src = {.stages = first.scope.stages},
            .dst = first.scope,
            .newLayout = first.layout,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .firstMipLevel = mipLevel,
            .mipLevels = 1,
        };
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
                _jobs,
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
            stageSlot.previous =
                _frameSlots[(slot + frameSlots - 1) % frameSlots].stageSlots[stageId];

            for(auto const& wait : stages[stageId].waits) {
                auto const waitedSlot =
                    (slot + frameSlots - wait.frameOffset % frameSlots) % frameSlots;
                auto const& waited = _frameSlots[waitedSlot].stageSlots[wait.stageId];

                // the first frame of the slot waits for the frame frameOffset frames before it
                stageSlot.slotSubmit.waits.push_back(
                    SemaphoreSubmitInfo{
                        .semaphore = waited.semaphore,
                        .stages = wait.stages,
                        .value = _firstFrameNumber + slot - wait.frameOffset,
                    }
                );
            }
        }
}

StageGraph::~StageGraph() {
    // the waiters of an unsubmitted stage would hang in _jobs.wait(), e.g. after an exception in the recording
    for(auto& frameSlot : _frameSlots)
        for(auto& stage : frameSlot.stages) {
            if(stage.status() != Stage::Status::UNSUBMITTED)
                continue;
            // the binary waits dropped: a presenter is destroyed before the graph, its present
            // submits wait for the semaphores of the graph, the acquire semaphore of an unpresented
            // image is destroyed already
            stage._stageSlot->extraBinaryWaits.clear();
            try { skip(stage); }
            catch(std::exception const& e) {
                JVK_STABLE_ERR(
                    true,
                    "failed to skip stage {} in ~StageGraph(), got {}",
                    stage.id(),
                    e.what()
                ) {}
            }
        }

    _jobs.wait();

    for(auto& frameSlot : _frameSlots)
        for(auto& stageSlot : frameSlot.stageSlots) {
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
    if(_jobs.failed())
        _jobs.rethrow();

    auto const number = ++_frameNumber;
    auto const slotOf = [this](uint64_t frameNumber) -> dev::FrameSlot& {
        return _frameSlots[(frameNumber - _firstFrameNumber) % _config.frameSlots];
    };

    // the frame before ends now: its gpu work and its waiters do not wait for the reuse of its slot
    if(number > _firstFrameNumber)
        skipUnsubmitted(slotOf(number - 1));

    auto& frameSlot = slotOf(number);
    if(frameSlot.frameOpt.has_value()) {
        // a failed skip in an earlier nextFrame() leaves a stage of an older frame unsubmitted
        skipUnsubmitted(frameSlot);

        // the marks first: the gpu can finish before the graph job marks the submit
        co_await cth::co::all(
            frameSlot.stages
            | std::views::transform([](Stage const& stage) { return stage.submitted(); })
        );
        co_await cth::co::all(frameSlot.stages);
    }

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

    endPrimary(stageSlot.primary);
    detachJob(stage, Stage::Status::SUBMITTED);
}

void StageGraph::skip(Stage& stage) {
    auto& stageSlot = *stage._stageSlot;
    CTH_CRITICAL(&*stageSlot.graph != this, "the stage must be of this graph") {}
    CTH_CRITICAL(stage._status != Stage::Status::UNSUBMITTED, "the stage must be unsubmitted") {}

    // the recording of a begun primary is discarded, the next begin resets it
    endPrimary(stageSlot.primary);
    detachJob(stage, Stage::Status::SKIPPED);
}

void StageGraph::assume(cvk::Image const& image, ImageState const& state) {
    cvk::Image::debug_check(image);

    std::scoped_lock const lock{_statesMutex};
    _states.assume(image, state);
}

void StageGraph::erase(cvk::Image const& image) {
    cvk::Image::debug_check(image);

    std::scoped_lock const lock{_statesMutex};
    eraseStates(image.get(), image.mipLevels());
}

void StageGraph::erase(cvk::BaseBuffer const& buffer) {
    cvk::BaseBuffer::debug_check(buffer);

    std::scoped_lock const lock{_statesMutex};
    eraseStates(buffer.get());
}

void StageGraph::endPrimary(PrimaryCmdBuffer& primary) {
    if(primary.rendering() != nullptr)
        primary.endRendering();
    primary.optEnd();
}

void StageGraph::skipUnsubmitted(dev::FrameSlot& frameSlot) {
    if(!frameSlot.frameOpt.has_value())
        return;

    for(auto& stage : frameSlot.stages) {
        if(stage.status() != Stage::Status::UNSUBMITTED)
            continue;

        bool const waited = std::ranges::any_of(
            _config.stages,
            [&stage](StageConfig const& config) {
                return std::ranges::any_of(
                    config.waits,
                    [&stage](StageWait const& wait) { return wait.stageId == stage.id(); }
                );
            }
        );
        CTH_WARN(
            waited,
            "stage {} of frame {} has no submit or skip, nextFrame() skips it, its waiters waited until now",
            stage.id(),
            stage.frameNumber()
        ) {}

        skip(stage);
    }
}

void StageGraph::detachJob(Stage& stage, Stage::Status targetStatus) {
    auto& stageSlot = *stage._stageSlot;

    // the status after the detach: a failed allocation leaves the stage unsubmitted, the moved data is lost
    dev::RetainedResources retained{};
    Stasis::pos stasisEnd{};
    {
        std::scoped_lock const lock{stageSlot.stasisMutex};
        retained = std::move(stage._retained);
        stasisEnd = stageSlot.stasis.end();
    }

    core().executor().detach(
        _jobs,
        submitJob(
            stageSlot,
            stage._frameNumber,
            std::move(retained),
            stasisEnd,
            targetStatus
        )
    );
    stage._status = targetStatus;
}

cth::co::executor_task<void> StageGraph::submitJob(
    dev::StageSlot& stageSlot,
    uint64_t frameNumber,
    dev::RetainedResources retained,
    Stasis::pos stasisEnd,
    Stage::Status targetStatus
) {
    auto& slotSubmit = stageSlot.slotSubmit;
    auto& info = stageSlot.submitInfo;
    auto const& queue = *stageSlot.queue;

    // the marks first, off the strand: a wait before the signal on one queue would block it, the appends follow the
    // dependency order
    co_await stageSlot.previous->semaphore.submitted(frameNumber - 1);
    for(auto const& wait : slotSubmit.waits)
        co_await wait.semaphore->submitted(wait.value);
    for(auto const& wait : stageSlot.extraWaits)
        co_await wait.semaphore->submitted(wait.value);

    // the values of this frame, then next(): the job of the next frame of the slot reads them after
    // the mark
    // noexcept: without the skip info no fallback marks the slot, the waiters would hang
    [&]() noexcept {
        slotSubmit.skipInfo(info, stageSlot.extraWaits, stageSlot.extraBinaryWaits);
    }();
    // the stage of the next frame of the slot adds its own
    stageSlot.extraWaits.clear();
    stageSlot.extraBinaryWaits.clear();
    slotSubmit.next(_config.frameSlots);

    bool submitted = false;
    if(targetStatus == Stage::Status::SUBMITTED) {
        try {
            cvk::Barrier prologue{};
            release_map_t releases{};
            {
                std::scoped_lock const lock{_statesMutex};
                append(stageSlot.primary.states(), queue, prologue, releases);
            }
            // the skip of a failed submit waits for the releases too
            if(!releases.empty())
                info.waits.append_range(co_await release(std::move(releases)));

            // off the strand: the prologue belongs to the job of the slot, an empty one stays out of the submit
            bool const withPrologue = !prologue.empty();
            if(withPrologue) {
                stageSlot.prologue.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
                prologue.execute(stageSlot.prologue);
                stageSlot.prologue.end();
            }

            slotSubmit.addCmdBuffers(info, withPrologue);
            co_await queue.submit(info);
            submitted = true;
        }
        // the job goes on with the skip
        catch(...) { _jobs.store(std::current_exception()); }
    }

    // a skip, also of a failed submit: the gpu waits of other stages and the waits in ~StageGraph() end
    if(!submitted) {
        // noexcept: a failed start of the fallback leaves the slot unmarked, the waiters would hang
        auto fallback = [&]() noexcept {
            info.cmdBuffers.clear();
            return dev::fallback_submit(queue, info, _jobs);
        }();
        co_await std::move(fallback);
    }

    // after the mark: a failed gpu wait escapes, _jobs stores it, the retained objects stay for
    // the destruction of the graph
    // the job never touches the stage: nextFrame() destroys the stages after the gpu wait
    co_await stageSlot.semaphore.wait(frameNumber);

    {
        std::scoped_lock const lock{_statesMutex};
        for(auto const& [vkImage, mipLevels] : retained.images)
            eraseStates(vkImage, mipLevels);
        for(auto const vkBuffer : retained.buffers)
            eraseStates(vkBuffer);
    }

    std::scoped_lock const lock{stageSlot.stasisMutex};
    stageSlot.stasis.clear(stasisEnd);
}

void StageGraph::append(
    ResourceTracker const& recording,
    Queue const& queue,
    cvk::Barrier& prologue,
    release_map_t& releases
) {
    auto const family = queue.vk().familyIndex();

    // the last queue if it has another family, nullptr -> no last queue or the same family
    auto const otherFamilyQueue = [family
        ](auto const& lastQueues, auto const& key) -> Queue const* {
        auto const it = lastQueues.find(key);
        if(it == lastQueues.end() || it->second->vk().familyIndex() == family)
            return nullptr;
        return &it->second.get();
    };

    for(auto const& [vkImage, mipLevel, aspectMask, sharingMode] : recording.images()) {
        auto const firstOpt = recording.first(vkImage, mipLevel);
        // an assume() before the first use: ResourceTracker::append() takes the state
        if(!firstOpt.has_value())
            continue;

        auto const lastOpt = _states.last(vkImage, mipLevel);
        if(!lastOpt.has_value()) {
            prologue.add(vkImage, aspectMask, from_undefined(*firstOpt, mipLevel));
            continue;
        }

        auto const* lastQueuePtr = otherFamilyQueue(_imageQueues, image_mip_t{vkImage, mipLevel});
        if(lastQueuePtr == nullptr || sharingMode != VK_SHARING_MODE_EXCLUSIVE)
            continue;

        // ResourceTracker::append() inserts the state of the recording without a barrier
        _states.erase(vkImage, mipLevel);

        if(recording.discards(vkImage, mipLevel) || lastOpt->layout == VK_IMAGE_LAYOUT_UNDEFINED) {
            prologue.add(vkImage, aspectMask, from_undefined(*firstOpt, mipLevel));
            continue;
        }

        auto const lastFamily = lastQueuePtr->vk().familyIndex();
        releases[lastQueuePtr].add(
            vkImage,
            aspectMask,
            dev::release_half(*lastOpt, *firstOpt, lastFamily, family, mipLevel)
        );
        prologue.add(
            vkImage,
            aspectMask,
            dev::acquire_half(*lastOpt, *firstOpt, lastFamily, family, mipLevel)
        );
    }

    for(auto const& [vkBuffer, sharingMode] : recording.buffers()) {
        auto const firstOpt = recording.first(vkBuffer);
        auto const lastOpt = _states.last(vkBuffer);
        // unknown buffer: no barrier
        if(!firstOpt.has_value() || !lastOpt.has_value())
            continue;

        auto const* lastQueuePtr = otherFamilyQueue(_bufferQueues, vkBuffer);
        if(lastQueuePtr == nullptr || sharingMode != VK_SHARING_MODE_EXCLUSIVE)
            continue;

        _states.erase(vkBuffer);

        auto const lastFamily = lastQueuePtr->vk().familyIndex();
        releases[lastQueuePtr].add(vkBuffer, dev::release_half(*lastOpt, lastFamily, family));
        prologue.add(vkBuffer, dev::acquire_half(*firstOpt, lastFamily, family));
    }

    _states.append(recording, prologue);

    for(auto const& [vkImage, mipLevel, aspectMask, sharingMode] : recording.images())
        _imageQueues.insert_or_assign(
            image_mip_t{vkImage, mipLevel},
            cth::ref_wrapper<Queue const>{queue}
        );
    for(auto const& [vkBuffer, sharingMode] : recording.buffers())
        _bufferQueues.insert_or_assign(vkBuffer, cth::ref_wrapper<Queue const>{queue});
}

cth::co::executor_task<vector<SemaphoreSubmitInfo>> StageGraph::release(release_map_t releases) {
    vector<SemaphoreSubmitInfo> waits{};
    waits.reserve(releases.size());
    for(auto& [queuePtr, barrier] : releases)
        waits.push_back(co_await queuePtr->release(std::move(barrier)));
    co_return waits;
}

void StageGraph::eraseStates(VkImage vkImage, uint32_t mipLevels) {
    for(uint32_t level = 0; level < mipLevels; ++level) {
        _states.erase(vkImage, level);
        _imageQueues.erase(image_mip_t{vkImage, level});
    }
}

void StageGraph::eraseStates(VkBuffer vkBuffer) {
    _states.erase(vkBuffer);
    _bufferQueues.erase(vkBuffer);
}
}
