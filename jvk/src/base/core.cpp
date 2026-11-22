#include "jvk/base/core.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/base/queue_sets.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/surface/surface.hpp"
#include "cvk/utility/format.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <exception>
#include <memory>
#include <span>
#include <thread>
#include <utility>


namespace jvk {
using std::span;

Core::Core(cvk::Instance const& instance, Config config) :
    _config{std::move(config)},
    _corePtr{std::make_unique<cvk::Core>(instance)},
    _workers{
        _config.workers == 0
            ? std::max(std::thread::hardware_concurrency(), 1u)
            : _config.workers
    },
    _reactor{*_corePtr},
    _resourceHeap{*_corePtr, _config.heaps.resources} {}

Core::Core(
    cvk::Instance const& instance,
    Config config,
    span<cth::ref_wrapper<cvk::Surface const> const> surfaces
) :
    Core{instance, std::move(config)} { create(surfaces); }

Core::~Core() { optDestroy(); }

void Core::create(span<cth::ref_wrapper<cvk::Surface const> const> surfaces) {
    cvk::CoreConfig::debug_check(_config.vk);

    optDestroy();

    auto config = _config.vk;
    if(config.queueSets.empty())
        config.queueSets = queue_sets(config.queueProperties);
    config.features.merge(constants::DEVICE_FEATURES);

    _corePtr->create(surfaces, config);

    // the cvk core exists before the workers, the reactor and the queues, only a catch can destroy it when a later
    // step fails
    try {
        _workers.start();
        _reactor.create();
        createQueues();
        if(_config.heaps.createsResourceHeap())
            _resourceHeap.create();
        if(_config.heaps.createsSamplerHeap())
            _samplerHeapOpt.emplace(*_corePtr, _config.heaps.samplers);
    }
    catch(...) {
        _resourceHeap.optDestroy();
        destroyQueues();
        _reactor.optDestroy();
        _workers.await_stop();
        _corePtr->destroy();
        throw;
    }
}

void Core::destroy() {
    debug_check(*this);
    CTH_CRITICAL(_workers.owns_thread(), "destroy() must not run on a worker of the core") {}


    // one wait per vulkan queue: the first index of each vulkan queue
    // swallow errors bc destructor
    auto const indices = _corePtr->queueIndices();
    for(size_t qIdx = 0; qIdx < indices.size(); ++qIdx) {
        if(std::ranges::contains(indices.first(qIdx), indices[qIdx]))
            continue;

        // a failed wait does not stop the teardown, vulkan destroys do not fail, also after a device loss
        try { queue(qIdx).waitIdle(); }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(
                true,
                "failed to wait for queue {} in destroy(), got {}",
                qIdx,
                e.vkResult()
            ) {}
        }
        catch(std::exception const& e) {
            JVK_STABLE_ERR(
                true,
                "failed to wait for queue {} in destroy(), got {}",
                qIdx,
                e.what()
            ) {}
        }
        catch(...) {
            JVK_STABLE_ERR(
                true,
                "failed to wait for queue {} in destroy(), got an unknown exception",
                qIdx
            ) {}
        }
    }

    destroyQueues();
    _samplerHeapOpt.reset();
    _resourceHeap.optDestroy();
    _reactor.destroy();
    _workers.await_stop();
    _corePtr->destroy();
}

Queue const& Core::queue(size_t queueIdx) const {
    debug_check(*this);
    CTH_CRITICAL(
        queueIdx >= _queueIndices.size(),
        "queue index must be < {}, got {}",
        _queueIndices.size(),
        queueIdx
    ) {}
    return _queues[_queueIndices[queueIdx]];
}

ResourceHeap const& Core::resourceHeap() const {
    debug_check(*this);
    CTH_CRITICAL(
        !_resourceHeap.created(),
        "the core must have a resource heap, see HeapsConfig"
    ) {}
    return _resourceHeap;
}

void Core::createQueues() {
    if(_config.driverThreads > 0)
        _driverSchedulerOpt.emplace(cth::co::autostart, _config.driverThreads);
    cth::co::scheduler const& scheduler = _driverSchedulerOpt.has_value()
        ? *_driverSchedulerOpt
        : _workers;

    auto const indices = _corePtr->queueIndices();
    _queues.reserve(indices.size());
    _queueIndices.reserve(indices.size());

    for(size_t queueIdx = 0; queueIdx < indices.size(); ++queueIdx) {
        auto const firstIdx =
            static_cast<size_t>(std::ranges::find(indices, indices[queueIdx]) - indices.begin());
        if(firstIdx < queueIdx) {
            _queueIndices.push_back(_queueIndices[firstIdx]);
            continue;
        }

        _queueIndices.push_back(_queues.size());
        _queues.emplace_back(*this, _corePtr->queue(queueIdx), scheduler);
    }
}

void Core::destroyQueues() {
    _queues.clear();
    _queueIndices.clear();
    _driverSchedulerOpt.reset();
}

SamplerHeap const& Core::samplerHeap() const {
    debug_check(*this);
    CTH_CRITICAL(
        !_samplerHeapOpt.has_value(),
        "the core must have a sampler heap, see HeapsConfig"
    ) {}
    return *_samplerHeapOpt;
}
} // namespace jvk
