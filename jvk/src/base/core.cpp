#include "jvk/base/core.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/base/queue_sets.hpp"

#include "cvk/surface/surface.hpp"
#include "cvk/utility/constant/os_constants.hpp"

#include <algorithm>
#include <span>
#include <thread>
#include <utility>


namespace jvk {
Core::Core(cvk::Instance const& instance, Config config) :
    _config{std::move(config)},
    _core{instance},
    _workers{_config.workers == 0 ? std::max(std::thread::hardware_concurrency(), 1u) : _config.workers} {}

Core::Core(cvk::Instance const& instance, Config config, cvk::create_t) : Core{instance, std::move(config)} {
    create();
}

Core::~Core() { optDestroy(); }

void Core::create() {
    cvk::CoreConfig::debug_check(_config.vk);

    optDestroy();

    auto config = _config.vk;
    if(config.queueSets.empty()) config.queueSets = queue_sets(config.queueProperties);
    for(auto const extension : cvk::constants::OS_FENCE_DEVICE_EXTENSIONS)
        if(!std::ranges::contains(config.extensions, extension)) config.extensions.emplace_back(extension);

    _core.create(std::span<cvk::Surface const>{}, config);

    // the cvk core exists before the workers and the queues, only a catch can destroy it when a later step fails
    try {
        _workers.start();
        _queues.emplace(_core, _config.queueWorkers);
    } catch(...) {
        _workers.await_stop();
        _core.destroy();
        throw;
    }
}

void Core::destroy() {
    debug_check(*this);
    CTH_CRITICAL(_workers.owns_thread(), "destroy() must not run on a worker of the core") {}

    // one wait per vulkan queue: the first role of each queue index
    auto const indices = _core.queueIndices();
    for(size_t role = 0; role < indices.size(); ++role) {
        if(std::ranges::contains(indices.first(role), indices[role])) continue;

        // a failed wait does not stop the teardown, vulkan destroys do not fail, also after a device loss
        try {
            queue(role).waitIdle();
        } catch(...) {}
    }

    _queues.reset();
    _workers.await_stop();
    _core.destroy();
}

Queue const& Core::queue(size_t role) const {
    debug_check(*this);
    return _queues->queue(role);
}
}
