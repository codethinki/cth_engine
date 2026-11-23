#include "jvk/graph/surface.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/graph/stage.hpp"
#include "jvk/graph/stage_graph.hpp"
#include "jvk/sync/family_transfer.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/queue/submit_info.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/sync/barrier.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/utility/vk_exceptions.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <cth/coro/exception.hpp>
#include <cth/coro/executor.hpp>
#include <cth/coro/scheduler.hpp>

#include <array>
#include <exception>
#include <utility>


namespace jvk {
Surface::Surface(Core const& core, Queue const& queue, Config config) :
    _core{core},
    _queue{queue},
    _config{std::move(config)},
    _surface{static_cast<cvk::Core const&>(core).instance()},
    _swapchain{core, queue.vk(), _surface},
    _semaphore{core} { Config::debug_check(_config); }

Surface::Surface(Core const& core, Queue const& queue, Config config, cvk::Surface surface) :
    Surface{core, queue, std::move(config)} { create(std::move(surface)); }

void Surface::create(cvk::Surface surface) {
    Core::debug_check(*_core);
    cvk::Surface::debug_check(surface);
    optDestroy();

    _surface = std::move(surface);
    cvk::Core const& vkCore = *_core;

    try {
        _semaphore.create();
        _acquireSemaphores.reserve(_config.framesInFlight);
        for(uint32_t i = 0; i < _config.framesInFlight; ++i)
            _acquireSemaphores.emplace_back(vkCore, cvk::create);

        auto const extent = _surface.extent(vkCore.physicalDevice());
        if(extent.width > 0 && extent.height > 0)
            updateSwapchain(extent, nullptr);
    }
    catch(...) {
        destroy();
        throw;
    }
}

void Surface::destroy() {
    debug_check(*this);

    _jobs.wait();

    // a failed wait leaves the gpu work, the destruction goes on
    if(_semaphore.created()) {
        try {
            cvk::TimelineSemaphore const& semaphore = _semaphore;
            semaphore.wait(_semaphore.value());
        }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(
                true,
                "failed to wait for the present submits in destroy(), got {}",
                e.vkResult()
            ) {}
        }
    }

    // after the wait for the present submits: the earlier frames that used the image ended on the
    // gpu (VUID-VkReleaseSwapchainImagesInfoKHR-pImageIndices-07786), the stage of the image is under
    // the @pre
    // no job runs, no acquire is pending: nothing else uses the swapchain
    if(_acquiredOpt.has_value()) {
        try { unacquireImage(); }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(
                true,
                "failed to unacquire the swapchain image in destroy(), got {}",
                e.vkResult()
            ) {}
            _acquiredOpt.reset();
        }
    }

    _submits.clear();
    _acquireSemaphores.clear();
    _semaphore.optDestroy();
    // the semaphore starts at 0 again
    _presented = cth::co::counter_event{};
    _swapchain.optDestroy();
    _surface.destroy();

    _acquireSemaphoreIdx = 0;
    _outOfDate = false;
    // after _jobs.wait(): no job stores anymore
    _jobs.clear();
}

cth::co::executor_task<scoped_acquire> Surface::acquire(
    Stage& stage,
    VkPipelineStageFlags2 stages
) {
    if(_jobs.failed())
        _jobs.rethrow();

    debug_check(*this);
    CTH_CRITICAL(stage.status() != Stage::Status::UNSUBMITTED, "the stage must be unsubmitted") {}
    CTH_CRITICAL(
        _acquiredOpt.has_value() && _acquiredOpt->frameNumber == stage.frameNumber()
        && _acquiredOpt->stageSemaphore->submittedValue() < _acquiredOpt->frameNumber,
        "a second acquire in frame {}: submit or skip the stage of the first image first",
        stage.frameNumber()
    ) {}

    // present N before acquire N + 1: the acquire and the present share the swapchain
    // (externally synchronized), a blocking acquire would delay the present (FIFO), the flag of
    // the present is final
    co_await _presented.wait(_semaphore.value());
    // the unacquire needs the gpu done with its stage: VUID-VkReleaseSwapchainImagesInfoKHR-pImageIndices-07786
    if(_acquiredOpt.has_value())
        co_await _acquiredOpt->stageSemaphore->wait(_acquiredOpt->frameNumber);
    // outside the strand: the ends of the present jobs order the swapchain uses
    co_await cth::co::executor{_queue->scheduler()}.schedule();

    if(_acquiredOpt.has_value())
        unacquireImage();

    cvk::Core const& vkCore = *_core;
    std::optional<uint32_t> imageIdxOpt = std::nullopt;
    auto const& acquireSemaphore = _acquireSemaphores[_acquireSemaphoreIdx];

    while(!imageIdxOpt.has_value()) {
        auto const extent = _surface.extent(vkCore.physicalDevice());
        if(extent.width == 0 || extent.height == 0)
            co_return scoped_acquire{*this, stage, std::nullopt, _jobs};
        updateSwapchain(extent, &stage.graph());

        // no image -> out of date, the next round recreates the swapchain
        imageIdxOpt = _swapchain.acquire(acquireSemaphore).imageIndex;
        if(!imageIdxOpt.has_value())
            _outOfDate = true;
    }

    auto const imageIdx = *imageIdxOpt;
    auto const& swapchain = _swapchain.current();
    SwapchainImage const image{
        .image = swapchain.image(imageIdx),
        .view = swapchain.view(imageIdx),
        .imageIdx = imageIdx,
        .extent = swapchain.extent(),
    };
    _acquireSemaphoreIdx = (_acquireSemaphoreIdx + 1) % _config.framesInFlight;
    _acquiredOpt = AcquiredImage{
        .image = image,
        .stageSemaphore = stage.semaphore(),
        .frameNumber = stage.frameNumber(),
    };

    // the first barrier of the image chains with the wait for the acquire
    stage.await(acquireSemaphore, stages);
    stage.graph().assume(
        *image.image,
        ImageState{.layout = VK_IMAGE_LAYOUT_UNDEFINED, .scope = {.stages = stages}}
    );

    co_return scoped_acquire{*this, stage, image, _jobs};
}

void Surface::present(Stage& stage) {
    debug_check(*this);
    CTH_CRITICAL(!_acquiredOpt.has_value(), "the surface must have an acquired image") {}
    CTH_CRITICAL(stage.status() != Stage::Status::SUBMITTED, "the stage must be submitted") {}

    auto const image = _acquiredOpt->image;
    auto const lastOpt = stage.primary().states().last(image.image->get(), 0);
    CTH_CRITICAL(
        !lastOpt.has_value(),
        "the stage recording must use the swapchain image {}",
        image.imageIdx
    ) {}

    // the counter after the detach: a failed allocation leaves no value that nobody signals
    auto const value = _semaphore.value() + 1;
    _core->executor().detach(
        _jobs,
        presentJob(stage.queue(), stage.semaphore(), stage.frameNumber(), image, *lastOpt, value)
    );
    static_cast<void>(_semaphore.next());
    _acquiredOpt.reset();
}

cth::co::executor_task<void> Surface::presentJob(
    Queue const& stageQueue,
    Semaphore const& stageSemaphore,
    uint64_t frameNumber,
    SwapchainImage image,
    ImageState lastState,
    uint64_t value
) {
    auto const& queue = *_queue;
    auto const stageFamily = stageQueue.vk().familyIndex();
    auto const family = queue.vk().familyIndex();
    bool const otherFamily = stageFamily != family;
    auto const vkImage = image.image->get();
    auto const aspectMask = image.image->aspectMask();

    // off the strand: a wait before the signal on one queue would block it, the release needs the submit of the stage,
    // the next acquire() sets the state of the image in the graph after the submit job of the stage appended its states
    co_await stageSemaphore.submitted(frameNumber);
    // the present jobs run one at a time, also a failed one: the job before ended, it wrote the
    // values of the command buffers, signaled the value before and wrote the flag
    co_await _presented.wait(value - 1);

    // after the job before: it used the info until its end
    // noexcept: without the wait and the signal no skip submit signals the value, the waiters
    // would hang
    auto& info = _submitInfo;
    [&]() noexcept {
        info.clear();
        info.waits.push_back({.semaphore = stageSemaphore, .value = frameNumber});
        info.signals.push_back({.semaphore = _semaphore, .value = value});
    }();

    try {
        ImageState const presentState{
            .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .scope = {.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
        };

        cvk::Barrier barrier{};
        if(otherFamily) {
            cvk::Barrier release{};
            release.add(
                vkImage,
                aspectMask,
                dev::release_half(lastState, presentState, stageFamily, family, 0)
            );
            info.waits.push_back(co_await stageQueue.transfer(std::move(release)));

            barrier.add(
                vkImage,
                aspectMask,
                dev::acquire_half(lastState, presentState, stageFamily, family, 0)
            );
        } else
            barrier.add(
                vkImage,
                aspectMask,
                cvk::ImageBarrierInfo{
                    .src = lastState.scope,
                    .dst = presentState.scope,
                    .newLayout = presentState.layout,
                    .oldLayout = lastState.layout,
                }
            );

        // the last submit of the command buffer, a swapchain before a recreation used it too
        co_await _semaphore.wait(_submits[image.imageIdx].value);

        // off the strand: the present jobs run one at a time, the end of the job before orders them
        auto& submit = _submits[image.imageIdx];
        submit.cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        barrier.execute(submit.cmd);
        submit.cmd.end();
        // before the submit: the skip submit of a failed one signals the value too
        submit.value = value;
        info.cmdBuffers.push_back(submit.cmd);

        // the swapchain of the image is the current one: a recreation only happens in acquire(),
        // after the end of the last present job
        std::array const images{
            PresentImage{.swapchain = _swapchain.current(), .imageIdx = image.imageIdx}
        };
        std::array<VkResult, 1> results{};
        co_await queue.present(info, images, results);
        auto const result = results[0];
        // before _presented.set(): the next acquire() reads it
        if(result == VK_ERROR_OUT_OF_DATE_KHR)
            _outOfDate = true;
    }
    // the job goes on with the skip submit and _presented.set()
    catch(...) { _jobs.store(std::current_exception()); }

    // a failed submit marks nothing: the skip submit signals the value for the waits on the
    // surface
    if(_semaphore.submittedValue() < value) {
        // noexcept: a failed start of the skip submit leaves the value unmarked, the waiters would
        // hang
        auto skip = [&]() noexcept {
            info.cmdBuffers.clear();
            return dev::skip_submit(queue, info, _jobs);
        }();
        co_await std::move(skip);
    }

    // the last step, also after a failure: nothing before it escapes, the waiters would hang
    // one set() at a time, rising: the next job waits for this value
    _presented.set(value);
}

void Surface::updateSwapchain(VkExtent2D extent, StageGraph* graphPtr) {
    if(_swapchain.created() && !_outOfDate && _swapchain.current().extent() == extent)
        return;

    if(_swapchain.created()) {
        auto const& old = _swapchain.current();
        if(graphPtr != nullptr)
            for(uint32_t imageIdx = 0; imageIdx < old.imageCount(); ++imageIdx)
                graphPtr->erase(old.image(imageIdx));
        _swapchain.recreate(extent);
    } else
        _swapchain.create(swapchainConfig(), extent);
    _outOfDate = false;

    while(_submits.size() < _swapchain.current().imageCount())
        _submits.push_back(
            PresentSubmit{.cmd = PrimaryCmdBuffer{*_core, _queue->vk().familyIndex(), cvk::create}}
        );
}

Swapchain::Config Surface::swapchainConfig() const {
    auto config = _config.swapchain;
    if(config.minImageCount != 0)
        return config;

    cvk::Core const& vkCore = *_core;
    config.minImageCount =
        _surface.capabilities(vkCore.physicalDevice()).minImageCount + _config.framesInFlight;
    return config;
}

void Surface::unacquireImage() {
    _swapchain.current().unacquire(std::array{_acquiredOpt->image.imageIdx});
    _acquiredOpt.reset();
}
}

namespace jvk {
scoped_acquire::scoped_acquire(
    Surface& surface,
    Stage& stage,
    std::optional<SwapchainImage> imageOpt,
    cth::co::catching_scope& jobs
) noexcept :
    _surfacePtr{&surface},
    _stage{stage},
    _imageOpt{std::move(imageOpt)},
    _jobs{jobs} {}

scoped_acquire::~scoped_acquire() {
    if(!_surfacePtr || !_imageOpt.has_value())
        return;

    auto& stage = *_stage;
    // skipped or unsubmitted: the next acquire() or destroy() unacquires the image
    if(stage.status() != Stage::Status::SUBMITTED)
        return;

    // a destructor does not throw and is no task: it stores the failure, the next acquire()
    // rethrows
    try { _surfacePtr->present(stage); }
    catch(...) { _jobs->store(std::current_exception()); }
}

void scoped_acquire::present() {
    debug_check(*this);
    CTH_CRITICAL(_stage->status() == Stage::Status::SKIPPED, "the stage must not be skipped") {}

    if(_stage->status() == Stage::Status::UNSUBMITTED)
        _stage->submit();
    _surfacePtr.release()->present(*_stage);
}

void scoped_acquire::unacquire() {
    debug_check(*this);
    // the next acquire() or destroy() unacquires the image
    static_cast<void>(_surfacePtr.release());
}
}
