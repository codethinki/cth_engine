#pragma once
#include "jvk/base/queue.hpp"
#include "jvk/base/stasis.hpp"
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/graph/presenter.hpp"
#include "jvk/sync/resource_tracker.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/base/queue/submit_info.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cth/coro/counter_event.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/move_ptr.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>


namespace cvk {
class Semaphore;
}

namespace jvk {
class StageGraph;
class scoped_submit;
}

namespace jvk::dev {
/**
 * prebuilt submit of a stage slot
 * @details
 * - the graph builds it once, at its construction
 * - holds the values of the next submit or skip of the slot
 * - fills the `SubmitInfo` of the next submit or skip: a skip has no command buffers
 */
struct SlotSubmit {
    /** `StageSlot::prologue`, `StageSlot::primary`, `StageSlot::epilogue` */
    std::vector<cth::ref_wrapper<PrimaryCmdBuffer const>> cmdBuffers{};

    // no queueWaits: the submission awaits the marks of every wait, its append follows the
    // dependency order
    /**
     * one per `StageConfig::waits`: `StageSlot::semaphore` of the waited stage slot
     * @details the waited stage slot: stage `StageWait::stageId`, `StageWait::frameOffset` frame slots before
     * @details value: a frame number, `< StageGraph::firstFrameNumber()` -> a frame before the first frame, reached
     *  from the start
     */
    std::vector<SemaphoreSubmitInfo> waits{};

    SemaphoreSubmitInfo signal; ///< `StageSlot::semaphore` with the frame number

    /**
     * fills the info with the skip of the slot: the waits and the signal, no command buffers
     * @details waits for `SlotSubmit::waits` and the extra waits, signals `SlotSubmit::signal`
     * @param[out] info gets the skip, `SubmitInfo::clear()` first
     * @param extraWaits the jvk waits of the stage
     * @param extraBinaryWaits the binary waits of the stage
     * @see `addCmdBuffers()`
     */
    void skipInfo(
        SubmitInfo& info,
        std::span<SemaphoreSubmitInfo const> extraWaits,
        std::span<cvk::SemaphoreSubmitInfo const> extraBinaryWaits
    ) const;

    /**
     * adds the command buffers of the submit, the prologue first, the epilogue last
     * @param[out] info gets the command buffers
     * @param prologue `false` -> without `StageSlot::prologue`: the prologue is empty
     * @param epilogue `false` -> without `StageSlot::epilogue`: no image to present
     */
    void addCmdBuffers(SubmitInfo& info, bool prologue, bool epilogue) const;

    /**
     * advances every wait and signal value to the next frame of the slot
     * @details never reads the waited semaphores: their submissions run at the same time, a read can see an old frame
     * @param frameSlots `StageGraphConfig::frameSlots`, the values rise by it
     */
    constexpr void next(size_t frameSlots);
};

/**
 * the images and buffers of `Stage::retain()` in one frame
 * @details the submission erases their graph states before it destroys them, see `StageGraph::erase()`
 */
struct RetainedResources {
    /** the handle and the mip levels of each image */
    std::vector<std::pair<VkImage, uint32_t>> images{};
    std::vector<VkBuffer> buffers{};
};

// a constructor, no aggregate: `SlotSubmit::signal` and `SlotSubmit::cmdBuffers` refer to members,
// the mutex does not move
/**
 * resources of a stage in one frame slot
 * @details
 * - owned by the graph, live as long as the graph
 * - the `Stage` of each frame uses them
 */
struct StageSlot {
    /**
     * creates the semaphore and the command buffers of the stage slot
     * @details
     * - the semaphore starts at `StageGraph::firstFrameNumber() - 1`
     * - `SlotSubmit::signal`: the semaphore with the first frame number of the slot
     * - the graph sets `StageSlot::previous` and the waits of `StageSlot::slotSubmit` once all
     *   slots exist
     * - `secondaries`: `StageConfig::secondaries` of the stage
     * @pre `StageGraph::firstFrameNumber()` is set
     * @throws cvk::vk_result_exception result of a create of the semaphore or the command buffers
     */
    StageSlot(
        StageGraph& graph,
        size_t stageId,
        size_t slot,
        Queue& queue,
        size_t secondaries
    );

    cth::ref_wrapper<StageGraph> graph;

    size_t stageId; ///< id of the stage, see `StageGraphConfig::stages`

    size_t slot;
    cth::ref_wrapper<Queue> queue;

    /**
     * the heaps of the core that the stage binds and that its secondaries inherit
     * @details `nullptr` -> the core has no such heap, or the family of the queue has no graphics and no compute
     */
    cvk::HeapInheritance heaps;

    /**
     * signaled by the submit or the skip of each frame of the slot
     * @details value: the frame number, rises by `StageGraphConfig::frameSlots` per signal
     * @details initial value: `StageGraph::firstFrameNumber() - 1`
     */
    Semaphore semaphore;

    // a member of the stage slot: it refers to StageSlot::semaphore, StageSlot::prologue,
    // StageSlot::primary and the semaphores of the waited stage slots, they live as long as the graph
    /** the waits and the signal of the next submit or skip, see `SlotSubmit::next()` */
    SlotSubmit slotSubmit;

    // the next three in the stage slot, not in the stage: they keep their capacity for the next
    // frames of the slot
    // one list per kind of wait: the jvk waits keep the jvk semaphore
    /** the jvk waits of `Stage::await()` in the frame of the slot, the submission clears them */
    std::vector<SemaphoreSubmitInfo> extraWaits{};

    /** the binary waits of `Stage::await()` in the frame of the slot, the submission clears them */
    std::vector<cvk::SemaphoreSubmitInfo> extraBinaryWaits{};

    /**
     * the images to present in the frame of the slot, see `Stage::present()`
     * @details the submission presents them after the submit or drops them after a skip, then
     *  clears the list
     */
    std::vector<Presenter::AcquiredImage> presents{};

    /**
     * the submit or the skip of the frame of the slot
     * @details
     * - the submission fills it, see `SlotSubmit::skipInfo()`
     * - one submission at a time: `StageGraph::nextFrame()` hands out the slot again after
     *   `StageSlot::submitDone` of the submission
     */
    SubmitInfo submitInfo{};

    // the scratch of the presents: the submission refills them, they keep their capacity
    /** the presents as uses of their images after the primary, appended to the graph states */
    ResourceTracker epilogueUses{};

    /**
     * the barriers of the uses in `StageSlot::epilogueUses`, recorded into `StageSlot::epilogue`
     * @details first in the epilogue
     */
    cvk::Barrier epilogueBarriers{};

    /**
     * the releases of the presented images to the queue family of their presenter queue
     * @details after `StageSlot::epilogueBarriers` in the epilogue
     */
    cvk::Barrier presentReleases{};

    /**
     * frame number of the last submission of the slot that ended its submit or skip, its
     *  presents and its drops
     * @details `StageGraph::nextFrame()` awaits it before it hands out the slot again
     * @details initial value: `StageGraph::firstFrameNumber() - 1`
     */
    cth::co::counter_event submitDone;

    /** the stage in the frame slot before, submits first */
    cth::ref_wrapper<StageSlot const> previous;

    PrimaryCmdBuffer prologue; ///< barriers for the first uses of the primary, submitted before it

    PrimaryCmdBuffer primary;

    /**
     * the uses of the presented images, submitted after the primary, see `Stage::present()`
     * @details records `StageSlot::epilogueBarriers`, then `StageSlot::presentReleases`
     */
    PrimaryCmdBuffer epilogue;

    std::vector<SecondaryCmdBuffer> secondaries{}; ///< `StageConfig::secondaries` of them

    /**
     * locks `StageSlot::stasis` and the retained resources of the stage, see `Stage::retain()`
     */
    std::mutex stasisMutex{};

    /**
     * the objects of `Stage::retain()`
     * @details the submission of a frame destroys them up to the `Stasis::end()` of its submit or
     *  skip
     */
    Stasis stasis;

public:
    StageSlot(StageSlot const& other) = delete;
    StageSlot(StageSlot&& other) = delete;
    StageSlot& operator=(StageSlot const& other) = delete;
    StageSlot& operator=(StageSlot&& other) = delete;
};
}

namespace jvk {
/**
 * submission of one stage in one frame
 * @details
 * - per frame: constructed by `StageGraph::nextFrame()`, handed out by `Frame::stage()`
 * - the resources come from its stage slot
 * - valid until `StageGraph::nextFrame()` hands out its slot again
 */
class Stage {
public:
    using Inheritance = SecondaryCmdBuffer::Inheritance;

    /**
     * what the stage did in its frame
     */
    enum class Status {
        UNSUBMITTED,
        SUBMITTED,
        SKIPPED,
    };

    /**
     * @param stageSlot resources of the stage in the frame slot
     */
    Stage(dev::StageSlot& stageSlot, uint64_t frameNumber);

    /**
     * begins the primary command buffer of the frame for the scope
     * @details resets the primary at each begin
     * @details binds the heaps of the core, see `StageSlot::heaps`
     * @return scope of the primary, submits at its end, see `scoped_submit`
     * @pre `status()` is `UNSUBMITTED`
     * @attention not thread safe
     */
    [[nodiscard]] scoped_submit begin();

    /**
     * primary command buffer of the frame slot
     * @details not begun
     * @see `begin()`
     */
    [[nodiscard]] PrimaryCmdBuffer& primary() { return _stageSlot->primary; }

    // std::optional instead of a default inheritance {}: an empty inheritance continues a rendering
    /**
     * begins the secondary command buffer of the frame slot
     * @details inherits the heaps of the primary, see `StageSlot::heaps`
     * @param inheritanceOpt `std::nullopt` -> for use outside of a rendering
     * @return the secondary
     * @pre the preconditions of `secondary()`
     * @attention not thread safe for one `secondaryIdx`
     */
    SecondaryCmdBuffer& beginSecondary(
        size_t secondaryIdx,
        std::optional<Inheritance> const& inheritanceOpt = std::nullopt
    );

    /**
     * secondary command buffer of the frame slot
     * @details not begun
     * @details the same index in the next frames of the slot is the same secondary
     * @pre `secondaryIdx < StageConfig::secondaries`
     * @pre `primary()` executes it before the `submit()`
     * @see `beginSecondary()`
     */
    [[nodiscard]] SecondaryCmdBuffer& secondary(size_t secondaryIdx);

    /**
     * secondary command buffer for the queue family of the stage, owned by the caller
     * @details not begun
     * @throws cvk::vk_result_exception result of `vkCreateCommandPool()` or `vkAllocateCommandBuffers()`
     * @see `SecondaryCmdBuffer::begin()`
     */
    [[nodiscard]] SecondaryCmdBuffer persistentSecondary() const;

    /**
     * adds a wait on a jvk semaphore to the submit of this frame
     * @details `submit()` and `skip()` wait for it, the submission first awaits its `Semaphore::submitted()`
     * @pre `status()` is `UNSUBMITTED`
     * @pre the semaphore lives until the gpu finished the submit
     * @attention not thread safe
     * @see `StageGraph::submit()`
     */
    void await(SemaphoreSubmitInfo const&);

    /**
     * adds a wait on a jvk semaphore to the submit of this frame
     * @details calls @ref await(SemaphoreSubmitInfo const&)
     * @param semaphore to wait for
     * @param stages of this submit that wait
     * @param value to wait for
     */
    void await(Semaphore const& semaphore, VkPipelineStageFlags2 stages, uint64_t value) {
        await(SemaphoreSubmitInfo{semaphore, stages, value});
    }

    // deleted: without it a jvk semaphore takes the binary overload through the private base of
    // cvk::TimelineSemaphore and fails with an inaccessible base error. no reason: MSVC lacks C++26 delete("...")
    /**
     * a jvk semaphore needs a value: `await(semaphore, stages, value)`
     */
    void await(
        Semaphore const&,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    ) = delete;

    /**
     * adds a wait on a binary semaphore to the submit of this frame
     * @details `submit()` and `skip()` wait for it, no await on the cpu: for binary semaphores like the acquire
     * @param binary semaphore to wait for
     * @param stages of this submit that wait
     * @pre `status()` is `UNSUBMITTED`
     * @pre the signal of the semaphore is submitted before the submit of this frame
     * @pre the semaphore lives until the gpu finished the submit
     * @attention not thread safe
     */
    void await(
        cvk::Semaphore const& binary,
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    );

    /**
     * presents the image after the submit of this frame
     * @details `Surface::acquire()` calls it for its present stage
     * @details the submission of the frame:
     * - the present is a use of the image after the primary, in the epilogue: the barrier to
     *   `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`, see `StageSlot::epilogue`
     * - another queue family of the last use: the last queue releases the image, the epilogue
     *   acquires it
     * - the submit signals `Presenter::AcquiredImage::presentSemaphore()`
     * - another queue family of the presenter queue: the epilogue releases the image, the
     *   presenter queue acquires it and signals the present semaphore, see `Queue::transfer()`
     * - after the submit: `Presenter::present()`
     * - a skip or a failed submit: `Presenter::drop()` with the signal of the skip as the ready
     *   point
     * @param image the stage holds it until its submission presents or drops it
     * @pre `status()` is `UNSUBMITTED`
     * @attention not thread safe
     */
    void present(Presenter::AcquiredImage image);

    /**
     * submits the primary of the frame
     * @details calls `StageGraph::submit()` of the graph of the stage
     * @details a failure: no throw, see `StageGraph::submit()`
     * @pre the preconditions of `StageGraph::submit()`
     */
    void submit() noexcept;

    /**
     * submits no work in this frame
     * @details calls `StageGraph::skip()` of the graph of the stage
     * @details a failure: no throw, see `StageGraph::submit()`
     * @pre the preconditions of `StageGraph::skip()`
     */
    void skip() noexcept;

    /**
     * keeps the object until the gpu finished the submit or the skip of this frame
     * @details
     * - releases the object at once, the stasis of the stage slot holds its handles
     * - the recordings keep no reference to the object: the caller can destroy it
     * - an image or a buffer: the submission erases its graph state before it destroys it
     * - thread safe
     * @pre `status()` is `UNSUBMITTED`
     * @pre its users on other queues finish before this submit: this submit waits for them
     * @pre the submit of this frame is its last use
     */
    template<class T> requires(!std::is_lvalue_reference_v<T>) && requires(T& object) {
        object.release().decompose();
    }
    void retain(T&& object) {
        CTH_CRITICAL(_status != Status::UNSUBMITTED, "the stage must be unsubmitted") {}

        std::scoped_lock const lock{_stageSlot->stasisMutex};
        if constexpr(std::derived_from<T, cvk::Image>)
            _retained.images.emplace_back(object.get(), object.mipLevels());
        else if constexpr(std::derived_from<T, cvk::BaseBuffer>)
            _retained.buffers.push_back(object.get());
        _stageSlot->stasis.add(object.release());
    }

    /**
     * waits until the gpu finished the submit or the skip of this frame: `co_await stage`
     * @details
     * - resumes on the context of the caller
     * - reached -> no suspend
     * - `Semaphore::wait()` of `semaphore()` with `frameNumber()`
     * @pre `status()` is not `UNSUBMITTED`
     */
    [[nodiscard]] constexpr semaphore_awaiter operator co_await() const {
        return _stageSlot->semaphore.wait(_frameNumber);
    }

    /**
     * waits until the submit or the skip of this frame reached `vkQueueSubmit2()`
     * @details
     * - not until the gpu finished
     * - resumes on the context of the caller
     * - reached -> no suspend
     * - await it before a submit on `queue()` that waits for `semaphore()`
     * - `Semaphore::submitted()` of `semaphore()` with `frameNumber()`
     * @see `operator co_await()`
     */
    [[nodiscard]] constexpr cth::co::counter_event::awaiter submitted() const {
        return _stageSlot->semaphore.submitted(_frameNumber);
    }

    [[nodiscard]] constexpr Status status() const { return _status; }
    [[nodiscard]] constexpr StageGraph& graph() const { return *_stageSlot->graph; }
    [[nodiscard]] constexpr size_t id() const { return _stageSlot->stageId; }
    [[nodiscard]] constexpr size_t slot() const { return _stageSlot->slot; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_stageSlot->queue; }

    /**
     * @return semaphore of the stage slot
     * @details the submit or the skip of frame N signals N
     */
    [[nodiscard]] constexpr Semaphore const& semaphore() const { return _stageSlot->semaphore; }

    /**
     * @return number of the frame, the value of `semaphore()` for this frame
     * @details the first frame is `StageGraph::firstFrameNumber()`
     */
    [[nodiscard]] constexpr uint64_t frameNumber() const { return _frameNumber; }

private:
    // the graph sets the status of the submission, there is no other access without a public setter
    friend class StageGraph;

    cth::ref_wrapper<dev::StageSlot> _stageSlot;
    uint64_t _frameNumber;
    Status _status = Status::UNSUBMITTED;

    dev::RetainedResources _retained{}; ///< under `StageSlot::stasisMutex`

public:
    Stage(Stage const& other) = delete;
    Stage(Stage&& other) noexcept = default;
    Stage& operator=(Stage const& other) = delete;
    Stage& operator=(Stage&& other) noexcept = default;
};
}

namespace jvk {
/**
 * begun primary of a stage for the scope, submits at its end
 * @details
 * - `Stage::begin()` returns it
 * - normal end: `Stage::submit()` if the stage is `UNSUBMITTED`
 * - exception: `Stage::skip()` if the stage is `UNSUBMITTED`
 * - a submitted or skipped stage, a moved-from scope: nothing
 * - a failure of the submit or the skip at the end: no throw, see `StageGraph::submit()`
 * - `->` and `*` reach the primary
 * @attention a coroutine destroyed while suspended with the scope alive submits the stage
 */
class scoped_submit {
public:
    /**
     * @details the uncaught exceptions of the calling thread at the construction tell the exception at the end
     * @param stage with the begun primary
     * @pre `Stage::begin()` began the primary
     */
    explicit scoped_submit(Stage& stage) noexcept :
        _stagePtr{&stage},
        _uncaughtExceptions{std::uncaught_exceptions()} {}

    ~scoped_submit();

    [[nodiscard]] PrimaryCmdBuffer* operator->() const { return &_stagePtr->primary(); }
    [[nodiscard]] PrimaryCmdBuffer& operator*() const { return _stagePtr->primary(); }

private:
    cth::move_ptr<Stage> _stagePtr;
    int _uncaughtExceptions;

public:
    scoped_submit(scoped_submit const& other) = delete;
    scoped_submit(scoped_submit&& other) noexcept = default;
    scoped_submit& operator=(scoped_submit const& other) = delete;
    scoped_submit& operator=(scoped_submit&& other) = delete;
};
}

namespace jvk::dev {
constexpr void SlotSubmit::next(size_t frameSlots) {
    for(auto& wait : waits)
        wait.value += frameSlots;
    signal.value += frameSlots;
}
}
