#pragma once
#include <cth/coro/executor.hpp>
#include <cth/coro/scheduler.hpp>

namespace jvk {
class Core;
struct CoreConfig;
}

namespace jly {
struct CoreConfig;
}

namespace jly {
class Core {
public:
    using Config = CoreConfig;
    using VkConfig = jvk::CoreConfig;

    Core(Config);
    Core(Config, VkConfig);

    void create(VkConfig);

    /**
     * @pre @ref created()
     */
    void destroy();

private:
    std::unique_ptr<jvk::Core> _handle;


    cth::co::scheduler _scheduler;

public:
    [[nodiscard]] bool created() const;
    [[nodiscard]] jvk::Core const& raw() const { return *_handle; }
    [[nodiscard]] cth::co::scheduler const& scheduler() const {
        return _scheduler;
    }

    [[nodiscard]] cth::co::executor co_executor() const {
        return cth::co::executor{_scheduler};
    }


    static void debug_check(Core const& core);
};
}


namespace jly {
inline void Core::debug_check(Core const& core) {
    CTH_CRITICAL(!core.created(), "core not created") {}
}
}
