#include "jvk/graph/surface.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"

#include "cvk/base/core.hpp"

#include <utility>


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

cth::co::executor_task<std::optional<Presenter::AcquiredImage>> Surface::acquire() {
    debug_check(*this);

    cvk::Core const& vkCore = *_core;
    // out of date -> the next round recreates the swapchain
    bool outOfDate = false;
    for(;;) {
        auto const extent = _surface.extent(vkCore.physicalDevice());
        if(extent.width == 0 || extent.height == 0)
            co_return std::nullopt;

        updateSwapchain(extent, outOfDate);
        auto imageOpt = co_await _swapchain.acquire();
        if(imageOpt.has_value())
            co_return std::move(imageOpt);
        outOfDate = true;
    }
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
