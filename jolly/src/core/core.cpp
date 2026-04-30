#include "jolly/core/core.hpp"

#include "jolly/core/core_config.hpp"
#include "jolly/render/submit/queue.hpp"
#include "jolly/utility/jvk_conversion.hpp"

#include "jvk/base/core.hpp"



namespace jly {
namespace {

    std::vector<jvk::CoreConfig::queue_set_t> createQueueSets(std::span<QueueProperties const> queues) {
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
    }
}



Core::Core(Config config) : _handle{std::make_unique<jvk::Core>()},
    _config{std::move(config)},
    _scheduler{_config.schedulerThreads},
    _queues{createQueues(_config.queues)} {}
Core::Core(Config config, VkConfig vk_config) : Core{std::move(config)} { create(std::move(vk_config)); }
void Core::create(VkConfig vk_config) {
    VkConfig config{
        _config.appName,
        _config.engineName,
        _config.requiredExtensions,
        jvk::DestructionQueueConfig{_config.destructionQueueTickDelay},
        {
            std::from_range,
            _config.queues | std::views::transform(
                [](QueueProperties queue_property) { return to_queue_family_properties(queue_property); }
            )
        },

        //TEMP left off here. queue distribution is still an issue. see https://aistudio.google.com/app/prompts/1RE4tQLGRVyz9gQGEH_mDPgKfAKt1c9-j.
        // vk queue already has mutex now, jvk core now correctly creates requested queues & maps requested -> unique ones
        // still need to expose created queues in the core
        // jly core still needs to wrap them in jly queues and use the bottom up merging of equal queues documented in @ref CoreConfig
    }


    _handle->create(std::move(vk_config));
    _scheduler.start();
}
void Core::destroy() {
    debug_check(*this);

    _scheduler.request_stop();
    _handle->destroy();
}
std::vector<Queue> Core::createQueues(std::span<QueueProperties const> properties) {
    return {
        std::from_range,
        properties | std::views::transform([](auto const& p) { return Queue{p}; })
    };
}
bool Core::created() const { return _handle->created(); }
}
