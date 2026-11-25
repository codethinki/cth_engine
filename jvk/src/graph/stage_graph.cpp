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
#include <array>
#include <exception>
#include <execution>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>


namespace jvk {
using std::span;
using std::vector;

namespace {
    [[nodiscard]] uint64_t first_frame_number(StageGraphConfig const& config) {
        uint64_t maxOffset = 0;
        for(auto const& stage : config.stages)
            for(auto const& wait : stage.waits)
                maxOffset = std::max<uint64_t>(maxOffset, wait.frameOffset);
        return maxOffset + 1;
    }

    constexpr ImageState PRESENT_STATE{
        .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .scope = {.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
    };

    /** the present as a use of the image, its state is `PRESENT_STATE` */
    constexpr ImageAccess PRESENT_ACCESS{
        .layout = PRESENT_STATE.layout,
        .stages = PRESENT_STATE.scope.stages,
        .access = PRESENT_STATE.scope.access,
        .mipLevels = 1,
    };

    /** `src` waits for the stages of the first use, no access: like `cvk::layout_transition()` */
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
    // the waiters of an unsubmitted stage would hang in _submissions.wait(): the frame of the last
    // nextFrame(), an exception outside of a scope, a failed submit() or skip()
    for(auto& frameSlot : _frameSlots)
        for(auto& stage : frameSlot.stages) {
            if(stage.status() != Stage::Status::UNSUBMITTED)
                continue;
            // the skip drops the presents of the stage: the surfaces outlive the graph
            skip(stage);
            // a failed skip leaves the stage unsubmitted: nothing marks it, its waiters and
            // _submissions.wait() would hang
            if(stage.status() == Stage::Status::UNSUBMITTED) {
                JVK_STABLE_ERR(
                    true,
                    "the skip of an unsubmitted stage in ~StageGraph() failed, got stage {} of "
                    "frame {}",
                    stage.id(),
                    stage.frameNumber()
                ) {}
                std::terminate();
            }
        }

    _submissions.wait();

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
    if(_submissions.failed())
        _submissions.rethrow();

    auto const slotOf = [this](uint64_t frameNumber) -> dev::FrameSlot& {
        return _frameSlots[(frameNumber - _firstFrameNumber) % _config.frameSlots];
    };
    if(_frameNumber >= _firstFrameNumber)
        for(auto const& stage : slotOf(_frameNumber).stages)
            CTH_CRITICAL(
                stage.status() == Stage::Status::UNSUBMITTED,
                "every stage of the frame before must be submitted or skipped, got stage {} of "
                "frame {}",
                stage.id(),
                _frameNumber
            ) {}

    auto const number = ++_frameNumber;
    auto& frameSlot = slotOf(number);
    if(frameSlot.frameOpt.has_value()) {
        // the submissions first: the gpu can finish before the submission marks the submit, the
        // submission uses the slot until it ended its presents
        // seq: the children only wait, a post buys nothing
        co_await cth::co::all(
            std::execution::seq,
            frameSlot.stages
            | std::views::transform(
                [](Stage const& stage) {
                    return stage._stageSlot->submitDone.wait(stage.frameNumber());
                }
            )
        );
        co_await cth::co::all(std::execution::seq, frameSlot.stages);
    }

    frameSlot.frameOpt.reset();
    frameSlot.stages.clear();
    for(auto& stageSlot : frameSlot.stageSlots)
        frameSlot.stages.emplace_back(stageSlot, number);
    frameSlot.frameOpt.emplace(frameSlot, number);

    co_return *frameSlot.frameOpt;
}

void StageGraph::submit(Stage& stage) noexcept {
    detachSubmission(stage, Stage::Status::SUBMITTED);
}

void StageGraph::skip(Stage& stage) noexcept { detachSubmission(stage, Stage::Status::SKIPPED); }

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

void StageGraph::detachSubmission(Stage& stage, Stage::Status targetStatus) noexcept {
    auto& stageSlot = *stage._stageSlot;
    CTH_CRITICAL(&*stageSlot.graph != this, "the stage must be of this graph") {}
    CTH_CRITICAL(stage._status != Stage::Status::UNSUBMITTED, "the stage must be unsubmitted") {}

    // a failure: the stage stays unsubmitted, the next nextFrame() rethrows the error,
    // ~StageGraph() skips the stage
    try {
        // a skip discards the recording of a begun primary, the next begin resets it
        endPrimary(stageSlot.primary);

        // the status after the detach: a failed allocation leaves the stage unsubmitted, the moved
        // data is lost
        dev::RetainedResources retained{};
        Stasis::pos stasisEnd{};
        {
            std::scoped_lock const lock{stageSlot.stasisMutex};
            retained = std::move(stage._retained);
            stasisEnd = stageSlot.stasis.end();
        }

        core().executor().detach(
            _submissions,
            submission(
                stageSlot,
                stage._frameNumber,
                std::move(retained),
                stasisEnd,
                targetStatus
            )
        );
        stage._status = targetStatus;
    }
    catch(...) { _submissions.store(std::current_exception()); }
}

cth::co::executor_task<void> StageGraph::submission(
    dev::StageSlot& stageSlot,
    uint64_t frameNumber,
    dev::RetainedResources retained,
    Stasis::pos stasisEnd,
    Stage::Status targetStatus
) {
    auto& slotSubmit = stageSlot.slotSubmit;
    auto& info = stageSlot.submitInfo;
    auto& queue = *stageSlot.queue;

    // the marks first, off the strand: a wait before the signal on one queue would block it, the appends follow the
    // dependency order
    co_await stageSlot.previous->semaphore.submitted(frameNumber - 1);
    for(auto const& wait : slotSubmit.waits)
        co_await wait.semaphore->submitted(wait.value);
    for(auto const& wait : stageSlot.extraWaits)
        co_await wait.semaphore->submitted(wait.value);

    // the values of this frame, then next(): the submission of the next frame of the slot reads
    // them after the mark
    // noexcept: without the skip info no skip submit marks the slot, the waiters would hang
    [&]() noexcept {
        slotSubmit.skipInfo(info, stageSlot.extraWaits, stageSlot.extraBinaryWaits);
    }();
    // the stage of the next frame of the slot adds its own
    stageSlot.extraWaits.clear();
    stageSlot.extraBinaryWaits.clear();
    slotSubmit.next(_config.frameSlots);

    // the submit or the skip comes after every use of the images: the ready point of a drop
    for(auto const& image : stageSlot.presents)
        info.signals.push_back(image.ready());

    bool submitted = false;
    if(targetStatus == Stage::Status::SUBMITTED) {
        try {
            cvk::Barrier prologue{};
            release_map_t releases{};
            {
                std::scoped_lock const lock{_statesMutex};
                append(stageSlot.primary.states(), queue, prologue, releases);
                // the presents after the primary: uses of the images in the epilogue
                auto& epilogueUses = stageSlot.epilogueUses;
                epilogueUses.clear();
                stageSlot.epilogueBarriers.clear();
                // the first use of each image in the tracker: no barrier
                for(auto const& image : stageSlot.presents)
                    epilogueUses.access(image.image(), PRESENT_ACCESS, stageSlot.epilogueBarriers);
                append(epilogueUses, queue, stageSlot.epilogueBarriers, releases);
            }
            presentBarriers(stageSlot);
            // the skip of a failed submit waits for the releases too
            if(!releases.empty())
                info.waits.append_range(co_await release(std::move(releases)));

            // off the strand: the prologue and the epilogue belong to the submission of the slot,
            // an empty one stays out of the submit
            bool const withPrologue = !prologue.empty();
            if(withPrologue) {
                stageSlot.prologue.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
                prologue.execute(stageSlot.prologue);
                stageSlot.prologue.end();
            }
            bool const withEpilogue =
                !stageSlot.epilogueBarriers.empty() || !stageSlot.presentReleases.empty();
            if(withEpilogue) {
                stageSlot.epilogue.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
                // two executes: two barriers of one image in one execute have no order
                stageSlot.epilogueBarriers.execute(stageSlot.epilogue);
                stageSlot.presentReleases.execute(stageSlot.epilogue);
                stageSlot.epilogue.end();
            }

            // the queue of the stage and the presenter queue of one family: the submit signals the
            // present semaphore, another family: the transfer to the presenter queue signals it
            auto const family = queue.vk().familyIndex();
            for(auto const& image : stageSlot.presents)
                if(image.presenter().queue().vk().familyIndex() == family)
                    info.binarySignals.emplace_back(image.presentSemaphore());

            slotSubmit.addCmdBuffers(info, withPrologue, withEpilogue);
            co_await queue.submit(info);
            submitted = true;
        }
        // the submission goes on with the skip
        catch(...) { _submissions.store(std::current_exception()); }
    }

    if(submitted)
        co_await present(stageSlot, frameNumber);
    // a skip, also of a failed submit: the gpu waits of other stages and the waits in ~StageGraph() end
    else {
        // noexcept: a failed start of the skip submit leaves the slot unmarked, the waiters would
        // hang
        auto skip = [&]() noexcept {
            info.cmdBuffers.clear();
            // the present semaphores of a failed submit: no present waits for them
            info.binarySignals.clear();
            return dev::skip_submit(queue, info, _submissions);
        }();
        co_await std::move(skip);

        // the skip signaled the ready points: the spares wait for them
        for(auto& image : stageSlot.presents)
            handBack(std::move(image));
        stageSlot.presents.clear();
    }

    // the last use of the slot by the submission, after the presents and the drops, before the
    // gpu wait: nextFrame() hands the slot out after it
    stageSlot.submitDone.set(frameNumber);

    // nothing to free: no gpu wait, the submission ends
    if(retained.images.empty() && retained.buffers.empty()) {
        std::scoped_lock const lock{stageSlot.stasisMutex};
        if(stageSlot.stasis.empty(stasisEnd))
            co_return;
    }

    // after the mark: a failed gpu wait escapes, _submissions stores it, the retained objects stay
    // for the destruction of the graph
    // the submission never touches the stage: nextFrame() destroys the stages after the gpu wait
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
    Queue& queue,
    cvk::Barrier& prologue,
    release_map_t& releases
) {
    auto const family = queue.vk().familyIndex();

    // the last queue if it has another family, nullptr -> no last queue or the same family
    auto const otherFamilyQueue = [family
        ](auto const& lastQueues, auto const& key) -> Queue* {
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

        auto* const lastQueuePtr = otherFamilyQueue(_imageQueues, image_mip_t{vkImage, mipLevel});
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

        auto* const lastQueuePtr = otherFamilyQueue(_bufferQueues, vkBuffer);
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
            cth::ref_wrapper<Queue>{queue}
        );
    for(auto const& [vkBuffer, sharingMode] : recording.buffers())
        _bufferQueues.insert_or_assign(vkBuffer, cth::ref_wrapper<Queue>{queue});
}

void StageGraph::presentBarriers(dev::StageSlot& stageSlot) {
    auto const family = stageSlot.queue->vk().familyIndex();
    auto& releases = stageSlot.presentReleases;
    releases.clear();

    for(auto const& image : stageSlot.presents) {
        auto const presentFamily = image.presenter().queue().vk().familyIndex();
        if(presentFamily == family)
            continue;
        cvk::Image const& vkImage = image.image();
        releases.add(
            vkImage.get(),
            vkImage.aspectMask(),
            dev::release_half(PRESENT_STATE, PRESENT_STATE, family, presentFamily, 0)
        );
    }
}

cth::co::executor_task<void> StageGraph::present(dev::StageSlot& stageSlot, uint64_t frameNumber) {
    auto const family = stageSlot.queue->vk().familyIndex();
    SemaphoreSubmitInfo const submitted{.semaphore = stageSlot.semaphore, .value = frameNumber};

    for(auto& image : stageSlot.presents) {
        auto& presenter = image.presenter();
        auto& presentQueue = presenter.queue();
        auto const presentFamily = presentQueue.vk().familyIndex();
        if(presentFamily == family) {
            erase(image.image());
            co_await presenter.present(std::move(image));
            continue;
        }

        // another family: the presenter queue acquires the image, the transfer signals its present
        // semaphore
        std::optional<SemaphoreSubmitInfo> transferredOpt = std::nullopt;
        try {
            cvk::Image const& vkImage = image.image();
            cvk::Barrier acquire{};
            acquire.add(
                vkImage.get(),
                vkImage.aspectMask(),
                dev::acquire_half(PRESENT_STATE, PRESENT_STATE, family, presentFamily, 0)
            );
            std::array const waits{submitted};
            std::array const signals{cvk::SemaphoreSubmitInfo{image.presentSemaphore()}};
            transferredOpt = co_await presentQueue.transfer(std::move(acquire), waits, signals);
        }
        // a failed transfer: the image goes back behind the submit that released it, the submit
        // signaled its ready point
        catch(...) { _submissions.store(std::current_exception()); }
        if(!transferredOpt.has_value()) {
            handBack(std::move(image));
            continue;
        }

        erase(image.image());
        co_await presenter.present(std::move(image));

        // the transfer waits for the semaphore of the slot: ~StageGraph() destroys it after the
        // submissions ended
        try { co_await transferredOpt->semaphore->wait(transferredOpt->value); }
        catch(...) { _submissions.store(std::current_exception()); }
    }
    stageSlot.presents.clear();
}

void StageGraph::handBack(Presenter::AcquiredImage image) {
    // the image leaves the graph: an acquire of the image assumes its state again
    erase(image.image());
    auto& presenter = image.presenter();
    presenter.drop(std::move(image));
}

cth::co::executor_task<vector<SemaphoreSubmitInfo>> StageGraph::release(release_map_t releases) {
    vector<SemaphoreSubmitInfo> waits{};
    waits.reserve(releases.size());
    for(auto& [queuePtr, barrier] : releases)
        waits.push_back(co_await queuePtr->transfer(std::move(barrier)));
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
