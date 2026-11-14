#include "jvk/graph/swapchain.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/queue/queue.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/surface/surface.hpp"

#include <utility>


namespace jvk {
Swapchain::Swapchain(cvk::Core const& core, cvk::Queue const& presentQueue, cvk::Surface const& surface) :
    _core{core}, _presentQueue{presentQueue}, _surface{surface} {}

void Swapchain::create(Config config, VkExtent2D extent) {
    optDestroy();

    _swapchains.emplace_back(*_core, *_presentQueue, *_surface, std::move(config), extent);
}

void Swapchain::recreate(VkExtent2D extent) {
    debug_check(*this);

    auto const& old = current();
    // emplace_back keeps the addresses of the other swapchains, a throw leaves the deque as it was
    _swapchains.emplace_back(*_core, *_presentQueue, *_surface, old.config(), extent, &old);
}

void Swapchain::destroy() {
    debug_check(*this);

    // the oldest first: the retired swapchains end before the current one
    while(!_swapchains.empty())
        _swapchains.pop_front();
}

cvk::Swapchain::Acquired Swapchain::acquire(cvk::Semaphore const& signal, uint64_t timeout) {
    debug_check(*this);

    while(retired() != 0 && !_swapchains.front().acquired() && _swapchains.front().presented())
        _swapchains.pop_front();

    return current().acquire(signal, timeout);
}
}
