#include "jolly/core/core.hpp"

#include "jolly/core/core_config.hpp"
#include "jolly/render/submit/queue.hpp"
#include "jolly/utility/jvk_conversion.hpp"

#include "jvk/base/core.hpp"

#include "jvk/base/destruction_queue.hpp"
#include "jvk/base/device.hpp"



namespace jly {
namespace {

    std::vector<jvk::CoreConfig::queue_set_t> create_queue_sets(std::span<QueueProperties const> queues) {
        std::vector<jvk::CoreConfig::queue_set_t> result{};
        result.emplace_back(queues.size()); //0, ..., N

        auto const rbegin = static_cast<ptrdiff_t>(queues.size() - 2);

        //merge eq
        for(auto i = rbegin; i >= 0; i++) {
            auto const current = static_cast<size_t>(i);
            auto const prev = static_cast<size_t>(i + 1);

            if(queues[current] != queues[prev])
                continue;
            result.push_back(result.back());
            result.back().merge(current, prev);
        }

        //merge subset.eq
        for(auto i = rbegin; i >= 0; i++) {
            auto const current = i;
            auto const prev = i + 1;

            if(!contains(queues[current], queues[prev]))
                continue;
            result.push_back(result.back());
            result.back().merge(current, prev);
        }

        //merge all
        result.push_back(result.back());
        for(size_t i = 0; i < queues.size(); i++)
            result.back().merge(0, i);

        return result;
    }

    std::vector<jvk::QueueFamilyProperties> to_queue_properties(std::span<QueueProperties const> queues) {
        return {
            std::from_range,
            queues | std::views::transform(
                [](QueueProperties queue_property) { return to_queue_family_properties(queue_property); }
            )
        };
    }
}



Core::Core(Config config) :
    _handle{std::make_unique<jvk::Core>()},
    _config{std::move(config)},
    _scheduler{_config.schedulerThreads},
    _queues{_config.queues.size()} {}

Core::Core(Config config, create_t) : Core{std::move(config)} { create(); }

Core::~Core() = default;
void Core::tickFrame() const { _handle->destructionQueue()->next(); }

void Core::create() {
    createHandle();

    _scheduler.start();
}
void Core::wait() {
    debug_check(*this);

    _scheduler.request_stop();
    _handle->device().waitIdle();
    _scheduler.await_stop();
}
void Core::destroy() {
    debug_check(*this);

    wait();
    _handle->destroy();
}

void Core::createHandle() {
    VkConfig const config{
        _config.appName,
        _config.engineName,
        _config.requiredExtensions,
        jvk::DestructionQueueConfig{_config.destructionQueueTickDelay},
        to_queue_properties(_config.queues),
        create_queue_sets(_config.queues),
    };

    _handle->create(config);
}
void Core::createQueues() {
    for(size_t i = 0; i < _queues.size(); i++)
        _queues[i].wrap(std::make_unique<jvk::Queue>(_handle->queue(i)));
}
bool Core::created() const { return _handle->created(); }
Queue const& Core::queue(size_t idx) const {
    CTH_CRITICAL(idx >= _queues.size(), "idx({}) out of bounds({})", idx, _queues.size()) {}
    return _queues[idx];
}
}
