#pragma once
#include <cth/coro/executor.hpp>
#include <cth/coro/scheduler.hpp>

#include "jolly/core/core_config.hpp"
#include "jolly/utility/types.hpp"

#include <memory>
#include <vector>

namespace jvk {
class Core;
}


namespace jly {
class Queue;

class Core {
public:
    using Config = CoreConfig;
    using VkConfig = jvk::CoreConfig;

    Core(Config = {});
    Core(Config, create_t);
    ~Core();

    void tickFrame() const;

    void create();

    /**
     * waits for all operations to finish
     */
    void wait();

    /**
     * @pre @ref created()
     * @details calls @ref wait()
     */
    void destroy();



private:
    void createHandle();
    void createQueues();

    std::unique_ptr<jvk::Core> _handle;
    Config _config;
    cth::co::scheduler _scheduler;


    std::vector<Queue> _queues;

public:
    [[nodiscard]] bool created() const;
    [[nodiscard]] jvk::Core const& raw() const { return *_handle; }
    [[nodiscard]] cth::co::scheduler const& scheduler() const { return _scheduler; }

    [[nodiscard]] cth::co::executor co_executor() const { return cth::co::executor{_scheduler}; }

    [[nodiscard]] Queue const& queue(size_t idx) const;


    static void debug_check(Core const& core);
};
}


namespace jly {
inline void Core::debug_check(Core const& core) {
    CTH_CRITICAL(!core.created(), "core not created") {}
}
}
