#include "jvk/base/core.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/base/queue_sets.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/surface/surface.hpp"
#include "cvk/utility/format.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <exception>
#include <span>
#include <thread>
#include <utility>


namespace jvk {
Core::Core(cvk::Instance const& instance, Config config) :
    _config{std::move(config)}, _core{instance},
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

    _core.create({}, config);

    // the cvk core exists before the workers and the queues, only a catch can destroy it when a later step fails
    try {
        _workers.start();
        _queuesOpt.emplace(_core, _config.queueWorkers);
    }
    catch(...) {
        _workers.await_stop();
        _core.destroy();
        throw;
    }
}

void Core::destroy() {
    debug_check(*this);
    CTH_CRITICAL(_workers.owns_thread(), "destroy() must not run on a worker of the core") {}


    // one wait per vulkan queue: the first index of each vulkan queue
    // swallow errors bc destructor
    auto const indices = _core.queueIndices();
    for(size_t qIdx = 0; qIdx < indices.size(); ++qIdx) {
        if(std::ranges::contains(indices.first(qIdx), indices[qIdx]))
            continue;

        // a failed wait does not stop the teardown, vulkan destroys do not fail, also after a device loss
        try {
            queue(qIdx).waitIdle();
        }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(true, "failed to wait for queue {} in destroy(), got {}", qIdx, e.vkResult()) {}
        }
        catch(std::exception const& e) {
            JVK_STABLE_ERR(true, "failed to wait for queue {} in destroy(), got {}", qIdx, e.what()) {}
        }
        catch(...) {
            JVK_STABLE_ERR(true, "failed to wait for queue {} in destroy(), got an unknown exception", qIdx) {}
        }
    }

    _queuesOpt.reset();
    _workers.await_stop();
    _core.destroy();
}

Queue const& Core::queue(size_t queueIdx) const {
    debug_check(*this);
    return _queuesOpt->queue(queueIdx);
}
} // namespace jvk
