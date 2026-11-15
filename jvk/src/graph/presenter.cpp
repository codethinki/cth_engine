#include "jvk/graph/presenter.hpp"

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
#include <cth/coro/this_coro.hpp>

#include <array>
#include <exception>
#include <utility>
#include <vector>


namespace jvk {
using std::vector;

Presenter::Presenter(Core const& core, Queue const& queue, Config config) :
    _core{core},
    _queue{queue},
    _config{std::move(config)},
    _surface{static_cast<cvk::Core const&>(core).instance()},
    _swapchain{core, queue.vk(), _surface},
    _semaphore{core} {
    Config::debug_check(_config);
}

Presenter::Presenter(Core const& core, Queue const& queue, Config config, cvk::Surface surface) :
    Presenter{core, queue, std::move(config)} {
    create(std::move(surface));
}

void Presenter::create(cvk::Surface surface) {
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
        if(extent.width > 0 && extent.height > 0) updateSwapchain(extent, nullptr);
    }
    catch(...) {
        destroy();
        throw;
    }
}

void Presenter::destroy() {
    debug_check(*this);

    _jobs.wait();

    // no job runs, no acquire is pending: nothing else uses the swapchain
    if(_imageOpt.has_value()) {
        try {
            unacquireImage();
        }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(true, "failed to unacquire the swapchain image in destroy(), got {}", e.vkResult()) {}
            _imageOpt.reset();
        }
    }

    // a failed wait leaves the gpu work, the destruction goes on
    if(_semaphore.created()) {
        try {
            cvk::TimelineSemaphore const& semaphore = _semaphore;
            semaphore.wait(_semaphore.value());
        }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(true, "failed to wait for the present submits in destroy(), got {}", e.vkResult()) {}
        }
    }

    _submits.clear();
    _acquireSemaphores.clear();
    _semaphore.optDestroy();
    _swapchain.optDestroy();
    _surface.destroy();

    _acquireSemaphoreIdx = 0;
    _presentedImageIndices.clear();
    _outOfDate = false;
    _error.clear();
}

cth::co::executor_task<scoped_acquire> Presenter::acquire(Stage& stage, VkPipelineStageFlags2 stages) {
    // on the calling thread, the scope compares them at its end
    auto const uncaughtExceptions = std::uncaught_exceptions();

    debug_check(*this);
    CTH_CRITICAL(stage.status() != Stage::Status::UNSUBMITTED, "the stage must be unsubmitted") {}

    // after the last present: the acquire and the present share the swapchain in the strand, a blocking acquire before
    // the last present delays that present by one refresh (FIFO), the order of a plain loop: present N, then acquire
    // N + 1
    auto const value = _semaphore.value();
    if(value > 0) co_await _semaphore.submitted(value);
    co_await _queue->executor().schedule();

    _error.rethrow();
    // an image without a present, e.g. after an exception
    if(_imageOpt.has_value()) unacquireImage();

    cvk::Core const& vkCore = *_core;
    std::optional<uint32_t> imageIdxOpt = std::nullopt;
    auto const& acquireSemaphore = _acquireSemaphores[_acquireSemaphoreIdx];

    while(!imageIdxOpt.has_value()) {
        auto const extent = _surface.extent(vkCore.physicalDevice());
        if(extent.width == 0 || extent.height == 0)
            co_return scoped_acquire{*this, stage, std::nullopt, _error, uncaughtExceptions};
        updateSwapchain(extent, &stage.graph());
        auto const& swapchain = _swapchain.current();

        // the ended presents free their images
        while(!_presentedImageIndices.empty() && swapchain.presentFence(_presentedImageIndices.front()).signaled())
            _presentedImageIndices.pop_front();
        if(_presentedImageIndices.size() >= swapchain.imageCount()) {
            co_await awaitPresentFence(_presentedImageIndices.front());
            _presentedImageIndices.pop_front();
        }

        // an infinite timeout: no image -> out of date, the next round recreates the swapchain
        imageIdxOpt = _swapchain.acquire(acquireSemaphore).imageIndex;
        if(!imageIdxOpt.has_value()) _outOfDate = true;
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
    _imageOpt = image;

    // the first barrier of the image chains with the wait for the acquire
    stage.await(acquireSemaphore, stages);
    stage.graph().assume(*image.image, ImageState{.layout = VK_IMAGE_LAYOUT_UNDEFINED, .scope = {.stages = stages}});

    co_return scoped_acquire{*this, stage, image, _error, uncaughtExceptions};
}

void Presenter::present(Stage& stage) {
    debug_check(*this);
    CTH_CRITICAL(!_imageOpt.has_value(), "the presenter must have an acquired image") {}
    CTH_CRITICAL(stage.status() != Stage::Status::SUBMITTED, "the stage must be submitted") {}

    auto const image = *_imageOpt;
    auto const lastOpt = stage.primary().states().last(image.image->get(), 0);
    CTH_CRITICAL(!lastOpt.has_value(), "the stage recording must use the swapchain image {}", image.imageIdx) {}

    // the counter after the detach: a failed allocation leaves no value that nobody signals
    auto const value = _semaphore.value() + 1;
    _core->executor().detach(
        _jobs,
        presentJob(stage.queue(), stage.semaphore(), stage.frameNumber(), image, *lastOpt, value)
    );
    static_cast<void>(_semaphore.next());
    _imageOpt.reset();
}

void Presenter::unacquire() {
    debug_check(*this);
    CTH_CRITICAL(!_imageOpt.has_value(), "the presenter must have an acquired image") {}

    // detached onto the strand: the strand runs it before the next acquire()
    _queue->executor().detach(
        _jobs,
        [](Presenter& presenter) -> cth::co::executor_task<void> {
            try {
                presenter.unacquireImage();
            }
            catch(cvk::vk_result_exception const&) {
                presenter._error.store(std::current_exception());
            }
            co_return;
        }(*this)
    );
}

cth::co::executor_task<void> Presenter::presentJob(
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

    vector<SemaphoreSubmitInfo> waits{{.semaphore = stageSemaphore, .value = frameNumber}};
    SemaphoreSubmitInfo const signal{.semaphore = _semaphore, .value = value};

    // off the strand: a wait before the signal on one queue would block it, the release needs the submit of the stage,
    // the next acquire() sets the state of the image in the graph after the submit job of the stage appended its states
    co_await stageSemaphore.submitted(frameNumber);
    // the present jobs submit and present in order, also a failed one: the job before wrote the values of the command
    // buffers and signals the value before
    co_await _semaphore.submitted(value - 1);

    bool submitted = false;
    try {
        ImageState const presentState{
            .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .scope = {.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
        };

        cvk::Barrier barrier{};
        if(otherFamily) {
            cvk::Barrier release{};
            release.add(vkImage, aspectMask, dev::release_half(lastState, presentState, stageFamily, family, 0));
            waits.push_back(co_await stageQueue.release(std::move(release)));

            barrier.add(vkImage, aspectMask, dev::acquire_half(lastState, presentState, stageFamily, family, 0));
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

        // no suspension in the strand: the presents follow the order of present()
        co_await queue.executor().schedule();

        auto& submit = _submits[image.imageIdx];
        submit.cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        barrier.execute(submit.cmd);
        submit.cmd.end();

        queue.vk().submit({
            .cmdBuffers = {submit.cmd},
            .waits = {waits.begin(), waits.end()},
            .signals = {cvk::SemaphoreSubmitInfo{_swapchain.current().presentSemaphore(image.imageIdx)}, signal},
        });
        submit.value = value;
        submitted = true;
    }
    catch(...) {
        _error.store(std::current_exception());
    }

    // a failed submit: the timeline signal for the waits on the presenter, the present semaphore stays unsignaled
    if(!submitted) {
        co_await queue.executor().schedule();
        dev::fallback_submit(
            [&] { queue.vk().submit({.waits = {waits.begin(), waits.end()}, .signals = {signal}}); },
            _error
        );
    }

    if(submitted) {
        try {
            // the swapchain of the image is the current one: a recreation only happens in acquire(), after the last
            // present called vkQueuePresentKHR()
            if(_swapchain.current().present(image.imageIdx) == VK_ERROR_OUT_OF_DATE_KHR) _outOfDate = true;
            _presentedImageIndices.push_back(image.imageIdx);
        }
        catch(cvk::vk_result_exception const&) {
            _error.store(std::current_exception());
        }
    }

    // after the present: the next acquire() awaits it
    _semaphore.markSubmitted(value);
}

cth::co::executor_task<void> Presenter::awaitPresentFence(uint32_t imageIdx) const {
    auto const handle = _swapchain.current().presentFence(imageIdx).extractOsHandle();
    // posix: no handle -> signaled
    if(!handle) co_return;
    co_await cth::co::this_coro::await_handle(handle.get());
}

void Presenter::updateSwapchain(VkExtent2D extent, StageGraph* graphPtr) {
    if(_swapchain.created() && !_outOfDate && _swapchain.current().extent() == extent) return;

    if(_swapchain.created()) {
        auto const& old = _swapchain.current();
        if(graphPtr != nullptr)
            for(uint32_t imageIdx = 0; imageIdx < old.imageCount(); ++imageIdx)
                graphPtr->erase(old.image(imageIdx));
        _swapchain.recreate(extent);
    } else _swapchain.create(swapchainConfig(), extent);
    _outOfDate = false;

    // the indices of the retired swapchain
    _presentedImageIndices.clear();
    while(_submits.size() < _swapchain.current().imageCount())
        _submits.push_back(PresentSubmit{.cmd = PrimaryCmdBuffer{*_core, _queue->vk().familyIndex()}});
}

Swapchain::Config Presenter::swapchainConfig() const {
    auto config = _config.swapchain;
    if(config.minImageCount != 0) return config;

    cvk::Core const& vkCore = *_core;
    config.minImageCount = _surface.capabilities(vkCore.physicalDevice()).minImageCount + _config.framesInFlight;
    return config;
}

void Presenter::unacquireImage() {
    _swapchain.current().unacquire(std::array{_imageOpt->imageIdx});
    _imageOpt.reset();
}
}

namespace jvk {
scoped_acquire::scoped_acquire(
    Presenter& presenter,
    Stage& stage,
    std::optional<SwapchainImage> imageOpt,
    dev::first_error& error,
    int uncaughtExceptions
) noexcept :
    _presenterPtr{&presenter},
    _stage{stage},
    _imageOpt{std::move(imageOpt)},
    _error{error},
    _uncaughtExceptions{uncaughtExceptions} {}

scoped_acquire::~scoped_acquire() {
    if(!_presenterPtr || !_imageOpt.has_value()) return;

    auto& stage = *_stage;
    // an exception after the submit: the frame is complete
    bool const exception = std::uncaught_exceptions() > _uncaughtExceptions;
    if(exception && stage.status() != Stage::Status::SUBMITTED) return;
    CTH_CRITICAL(
        stage.status() == Stage::Status::UNSUBMITTED && stage.primary().recording()
            && stage.primary().states().last(_imageOpt->image->get(), 0).has_value(),
        "the open primary of the stage uses the swapchain image {}, declare scoped_acquire before scoped_submit",
        _imageOpt->imageIdx
    ) {}

    // a destructor does not throw: the next acquire() rethrows
    try {
        if(stage.status() == Stage::Status::SUBMITTED) _presenterPtr->present(stage);
        else _presenterPtr->unacquire();
    }
    catch(...) {
        _error->store(std::current_exception());
    }
}

void scoped_acquire::present() {
    debug_check(*this);
    CTH_CRITICAL(_stage->status() == Stage::Status::SKIPPED, "the stage must not be skipped") {}

    if(_stage->status() == Stage::Status::UNSUBMITTED) _stage->submit();
    _presenterPtr.release()->present(*_stage);
}

void scoped_acquire::unacquire() {
    debug_check(*this);
    _presenterPtr.release()->unacquire();
}
}
