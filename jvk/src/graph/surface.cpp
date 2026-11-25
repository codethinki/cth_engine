#include "jvk/graph/surface.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/graph/stage.hpp"
#include "jvk/graph/stage_graph.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/base/core.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/coro/counter_event.hpp>
#include <cth/coro/executor.hpp>
#include <cth/coro/scheduler.hpp>

#include <array>
#include <exception>
#include <utility>


namespace jvk {
namespace {
    cth::co::executor_task<void> await_value(cth::co::counter_event const& event, uint64_t value) {
        co_await event.wait(value);
    }
}

Surface::Surface(Core const& core, Queue& queue, Config config) :
    _core{core},
    _queue{queue},
    _config{std::move(config)},
    _surface{static_cast<cvk::Core const&>(core).instance()},
    _swapchain{core, queue.vk(), _surface} { Config::debug_check(_config); }

Surface::Surface(Core const& core, Queue& queue, Config config, cvk::Surface surface) :
    Surface{core, queue, std::move(config)} { create(std::move(surface)); }

void Surface::create(cvk::Surface surface) {
    Core::debug_check(*_core);
    cvk::Surface::debug_check(surface);
    optDestroy();

    _surface = std::move(surface);
    cvk::Core const& vkCore = *_core;

    try {
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

    // the submission of the present stage resolves the last image
    if(_presentState.resolved.value() < _lastValue)
        _core->executor().sync_wait(await_value(_presentState.resolved, _lastValue));

    // the drop resolved after the gpu finished the stages of the image:
    // VUID-VkReleaseSwapchainImagesInfoKHR-pImageIndices-07786
    if(_presentState.dropped) {
        // a failed unacquire does not stop the destruction
        try { _swapchain.current().unacquire(std::array{_lastImageIdx}); }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(
                true,
                "failed to unacquire the swapchain image in destroy(), got {}",
                e.vkResult()
            ) {}
        }
    }

    _acquireSemaphores.clear();
    _swapchain.optDestroy();
    _surface.destroy();

    // the counter starts at 0 again
    _presentState = dev::PresentState{};
    _lastValue = 0;
    _lastImageIdx = 0;
    _acquireSemaphoreIdx = 0;
}

cth::co::executor_task<std::optional<SwapchainImage>> Surface::acquire(
    Stage& acquireStage,
    Stage& presentStage,
    VkPipelineStageFlags2 stages
) {
    debug_check(*this);
    CTH_CRITICAL(
        acquireStage.status() != Stage::Status::UNSUBMITTED
        || presentStage.status() != Stage::Status::UNSUBMITTED,
        "the acquire stage and the present stage must be unsubmitted"
    ) {}
    CTH_CRITICAL(
        &acquireStage.graph() != &presentStage.graph()
        || acquireStage.frameNumber() != presentStage.frameNumber(),
        "the acquire stage and the present stage must belong to one frame of one graph"
    ) {}

    cvk::Core const& vkCore = *_core;
    std::optional<uint32_t> imageIdxOpt = std::nullopt;
    auto const& acquireSemaphore = _acquireSemaphores[_acquireSemaphoreIdx];

    //TEMP the try only after the resolve of the last image: the present state holds one image, a
    // try while the last image presents needs the state of several images
    if(_presentState.resolved.value() >= _lastValue) {
        handleLastPresent();

        auto const extent = _surface.extent(vkCore.physicalDevice());
        if(extent.width == 0 || extent.height == 0)
            co_return std::nullopt;

        // a recreation takes the fallback: in the strand it would hold up the submits
        if(_swapchain.created() && !_presentState.outOfDate
            && _swapchain.current().extent() == extent) {
            auto const acquired = co_await _queue->tryAcquire(_swapchain, acquireSemaphore);
            imageIdxOpt = acquired.imageIndex;
            if(acquired.result == VK_ERROR_OUT_OF_DATE_KHR)
                _presentState.outOfDate = true;
        }
    }

    // the fallback: no image from the try, a new extent or out of date
    if(!imageIdxOpt.has_value()) {
        // the last image first: then no present of the surface is pending, the blocking acquire
        // and the recreation need no strand, a blocking acquire before that present would delay it
        // (FIFO)
        co_await _presentState.resolved.wait(_lastValue);
        // outside the strand: the resolve of the last image orders the swapchain uses
        co_await cth::co::executor{_queue->scheduler()}.schedule();
        handleLastPresent();

        while(!imageIdxOpt.has_value()) {
            auto const extent = _surface.extent(vkCore.physicalDevice());
            if(extent.width == 0 || extent.height == 0)
                co_return std::nullopt;
            updateSwapchain(extent, &acquireStage.graph());

            // no image -> out of date, the next round recreates the swapchain
            imageIdxOpt = _swapchain.acquire(acquireSemaphore).imageIndex;
            if(!imageIdxOpt.has_value())
                _presentState.outOfDate = true;
        }
    }

    auto const imageIdx = *imageIdxOpt;
    auto& swapchain = _swapchain.current();
    SwapchainImage const image{
        .image = swapchain.image(imageIdx),
        .view = swapchain.view(imageIdx),
        .imageIdx = imageIdx,
        .extent = swapchain.extent(),
    };
    _acquireSemaphoreIdx = (_acquireSemaphoreIdx + 1) % _config.framesInFlight;
    _lastImageIdx = imageIdx;
    auto const value = ++_lastValue;

    // the first barrier of the image chains with the wait for the acquire
    acquireStage.await(acquireSemaphore, stages);
    acquireStage.graph().assume(
        *image.image,
        ImageState{.layout = VK_IMAGE_LAYOUT_UNDEFINED, .scope = {.stages = stages}}
    );

    // a failure drops the image at once, no stage submitted a use of it: the next acquire
    // unacquires it
    presentStage.present(dev::PendingPresent{_presentState, value, *_queue, swapchain, imageIdx});

    co_return image;
}

void Surface::handleLastPresent() {
    // written before the resolve: the error of the present of the last image
    if(_presentState.error != nullptr)
        std::rethrow_exception(std::exchange(_presentState.error, nullptr));

    // the drop resolved after the gpu finished the stages of the image:
    // VUID-VkReleaseSwapchainImagesInfoKHR-pImageIndices-07786
    if(std::exchange(_presentState.dropped, false))
        _swapchain.current().unacquire(std::array{_lastImageIdx});
}

void Surface::updateSwapchain(VkExtent2D extent, StageGraph* graphPtr) {
    if(_swapchain.created() && !_presentState.outOfDate && _swapchain.current().extent() == extent)
        return;

    if(_swapchain.created()) {
        auto const& old = _swapchain.current();
        if(graphPtr != nullptr)
            for(uint32_t imageIdx = 0; imageIdx < old.imageCount(); ++imageIdx)
                graphPtr->erase(old.image(imageIdx));
        _swapchain.recreate(extent);
    } else
        _swapchain.create(swapchainConfig(), extent);
    _presentState.outOfDate = false;
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
}
