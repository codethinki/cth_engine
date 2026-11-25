#include "jvk/graph/surface.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/graph/stage.hpp"
#include "jvk/graph/stage_graph.hpp"

#include "cvk/base/core.hpp"
#include "cvk/data/img/image.hpp"

#include <utility>
#include <variant>


namespace jvk {
Surface::Surface(Core const& core, Queue& queue, Config config) :
    _core{core},
    _queue{queue},
    _config{std::move(config)},
    _surface{static_cast<cvk::Core const&>(core).instance()},
    _swapchain{core, queue, _surface, _config.framesInFlight} { Config::debug_check(_config); }

Surface::Surface(Core const& core, Queue& queue, Config config, cvk::Surface surface) :
    Surface{core, queue, std::move(config)} { create(std::move(surface)); }

void Surface::create(cvk::Surface surface) {
    Core::debug_check(*_core);
    cvk::Surface::debug_check(surface);
    optDestroy();

    _surface = std::move(surface);
    cvk::Core const& vkCore = *_core;

    try {
        auto const extent = _surface.extent(vkCore.physicalDevice());
        if(extent.width > 0 && extent.height > 0)
            updateSwapchain(extent, false);
    }
    catch(...) {
        destroy();
        throw;
    }
}

void Surface::destroy() {
    debug_check(*this);

    _swapchain.optDestroy();
    _surface.destroy();
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
    auto& graph = acquireStage.graph();

    std::optional<Presenter::AcquiredImage> imageOpt = std::nullopt;
    // out of date -> the next round recreates the swapchain
    bool outOfDate = false;
    while(!imageOpt.has_value()) {
        auto const extent = _surface.extent(vkCore.physicalDevice());
        if(extent.width == 0 || extent.height == 0)
            co_return std::nullopt;

        updateSwapchain(extent, outOfDate);
        imageOpt = co_await _swapchain.acquire(&graph);
        outOfDate = !imageOpt.has_value();
    }

    auto& image = *imageOpt;
    SwapchainImage const swapchainImage{
        .image = image.image(),
        .view = image.view(),
        .imageIdx = image.imageIdx(),
        .extent = image.extent(),
    };

    // the first barrier of the image chains with the wait
    if(auto const* const semaphorePtr =
        std::get_if<cth::ref_wrapper<cvk::Semaphore const>>(&image.wait()))
        acquireStage.await(**semaphorePtr, stages);
    else {
        auto ready = std::get<SemaphoreSubmitInfo>(image.wait());
        ready.stages = stages;
        acquireStage.await(ready);
    }
    graph.assume(
        *swapchainImage.image,
        ImageState{.layout = VK_IMAGE_LAYOUT_UNDEFINED, .scope = {.stages = stages}}
    );

    presentStage.present(std::move(image));
    co_return swapchainImage;
}

void Surface::updateSwapchain(VkExtent2D extent, bool outOfDate) {
    if(!_swapchain.created()) {
        _swapchain.create(swapchainConfig(), extent);
        return;
    }
    if(outOfDate || _swapchain.current().extent() != extent)
        _swapchain.recreate(extent);
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
