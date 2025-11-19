#include "jolly/core/core.hpp"

#include "jolly/core/core_config.hpp"

#include "jvk/base/core.hpp"

namespace jly {

Core::Core(Config config) : _handle{std::make_unique<jvk::Core>()},
    _scheduler{config.schedulerThreads} {}
Core::Core(Config config, VkConfig vk_config) : Core{std::move(config)} {
    create(std::move(vk_config));
}
void Core::create(VkConfig vk_config) {
    _handle->create(std::move(vk_config));
    _scheduler.start();
}
void Core::destroy() {
    debug_check(*this);

    _scheduler.request_stop();
    _handle->destroy();
}
bool Core::created() const { return _handle->created(); }
}
