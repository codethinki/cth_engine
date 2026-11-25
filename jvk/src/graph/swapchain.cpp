#include "jvk/graph/swapchain.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/base/core.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <utility>


namespace jvk::dev {
PresentedSwapchain::PresentedSwapchain(
    cvk::Core const& core,
    Queue& presentQueue,
    cvk::Surface const& surface,
    cvk::Swapchain::Config config,
    VkExtent2D extent,
    cvk::Swapchain const* oldSwapchainPtr,
    uint32_t framesInFlight
) :
    swapchain{core, presentQueue.vk(), surface, std::move(config), extent, oldSwapchainPtr},
    presenter{presentQueue, swapchain, framesInFlight} {}
}

namespace jvk {
Swapchain::Swapchain(
    cvk::Core const& core,
    Queue& presentQueue,
    cvk::Surface const& surface,
    uint32_t framesInFlight
) :
    _core{core},
    _presentQueue{presentQueue},
    _surface{surface},
    _framesInFlight{framesInFlight} {
    CTH_CRITICAL(framesInFlight == 0, "framesInFlight must be > 0") {}
}

void Swapchain::create(Config config, VkExtent2D extent) {
    optDestroy();

    _swapchains.emplace_back(
        *_core,
        *_presentQueue,
        *_surface,
        std::move(config),
        extent,
        nullptr,
        _framesInFlight
    );
}

void Swapchain::recreate(VkExtent2D extent) {
    debug_check(*this);

    auto const& old = current();
    // emplace_back keeps the addresses of the other swapchains, a throw leaves the deque as it was
    try {
        _swapchains.emplace_back(
            *_core,
            *_presentQueue,
            *_surface,
            old.config(),
            extent,
            &old,
            _framesInFlight
        );
    }
    catch(cvk::vk_result_exception const&) {
        // vkCreateSwapchainKHR() retired the old swapchain, also on a failure
        _currentRetired = true;
        throw;
    }
    _currentRetired = false;
}

void Swapchain::destroy() {
    debug_check(*this);

    // the oldest first: the retired swapchains end before the current one
    while(!_swapchains.empty()) {
        // the swapchain destroys its present fences without a wait, a failed wait does not stop
        // the destruction: the device is lost
        try { _swapchains.front().presenter.waitPresents(); }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(
                true,
                "failed to wait for the present fences in destroy(), got {}",
                e.vkResult()
            ) {}
        }
        _swapchains.pop_front();
    }
    _currentRetired = false;
}

cth::co::executor_task<std::optional<Presenter::AcquiredImage>> Swapchain::acquire() {
    debug_check(*this);

    while(retired() != 0 && _swapchains.front().presenter.finished())
        _swapchains.pop_front();

    if(_currentRetired)
        co_return std::nullopt;
    co_return co_await presenter().acquire();
}
}
