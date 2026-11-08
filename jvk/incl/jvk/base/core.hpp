#pragma once
#include "core_config.hpp"
#include "queues.hpp"

#include "jvk/sync/timeline_reactor.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/constants.hpp"

#include <cth/coro/executor.hpp>
#include <cth/coro/scheduler.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <utility>


namespace cvk {
class Instance;
class Surface;
}

namespace jvk {
/**
 * jvk core
 * @details owns the cvk core, the workers, the timeline reactor and the serialized queues
 * @details the indices of one vulkan queue share one `Queue`
 */
class Core {
public:
    using Config = CoreConfig;

    /**
     * base constructor
     * @pre `cvk::Instance::created()`
     */
    Core(cvk::Instance const&, Config);

    //TODO review no default for the surfaces, like cvk::Core::create(): a core without windows passes {}
    /**
     * constructs and creates
     * @details calls @ref Core(cvk::Instance const&, Config), `create()`
     */
    Core(cvk::Instance const&, Config, std::span<cth::ref_wrapper<cvk::Surface const> const> surfaces);

    /**
     * constructs and creates with the surfaces of a braced list
     * @see @ref Core(cvk::Instance const&, Config, std::span<cth::ref_wrapper<cvk::Surface const> const>)
     */
    Core(
        cvk::Instance const& instance,
        Config config,
        std::initializer_list<cth::ref_wrapper<cvk::Surface const>> surfaces
    ) : Core{instance, std::move(config), std::span{surfaces}} {}

    //TEMP the old signature until core.cpp implements the surfaces, the tests use it
    explicit Core(cvk::Instance const&, Config, cvk::create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~Core();

    /**
     * creates the cvk core, the workers, the reactor and the queues
     * @param surfaces the present queues present to every surface
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     * @throws cvk::cvk_exception reason: `cvk::CoreConfig::device` is missing or does not meet the requirements
     * @throws cvk::vk_result_exception result of `vkCreateDevice()`
     * @throws cvk::vk_result_exception result of `vmaCreateAllocator()`
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()` for the reactor
     * @see `cvk::Core::create()`
     */
    void create(std::span<cth::ref_wrapper<cvk::Surface const> const> surfaces);

    /**
     * creates with the surfaces of a braced list
     * @see @ref create(std::span<cth::ref_wrapper<cvk::Surface const> const>)
     */
    void create(std::initializer_list<cth::ref_wrapper<cvk::Surface const>> surfaces) { create(std::span{surfaces}); }

    //TEMP the old signature until core.cpp implements the surfaces, the tests use it
    void create();

    /**
     * destroys the queues, the reactor, the workers and the cvk core
     * @details waits until every queue is idle first
     * @details a failed wait does not stop the destruction
     * @pre `created()`
     * @pre the calling thread runs no job of the core
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @param queueIdx index of the queue in `cvk::CoreConfig::queueProperties`
     * @return serialized queue, the indices of one vulkan queue return the same object
     * @pre `created()`
     * @pre `queueIdx` < size of `cvk::CoreConfig::queueProperties`
     */
    [[nodiscard]] Queue const& queue(size_t queueIdx) const;

    /**
     * @return executor of the workers
     * @pre `created()`
     */
    [[nodiscard]] constexpr cth::co::executor executor() const {
        debug_check(*this);
        return cth::co::executor{_workers};
    }

    /**
     * @return reactor of the core, waits for the timeline semaphores
     * @pre `created()`
     */
    [[nodiscard]] constexpr TimelineReactor const& reactor() const {
        debug_check(*this);
        return *_reactorOpt;
    }

    /**
     * the cvk core
     * @attention `cvk::Core::queue()` returns the queue without its strand
     */
    constexpr operator cvk::Core const&() const { return _core; }

    [[nodiscard]] constexpr bool created() const { return _core.created(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    Config _config;
    cvk::Core _core;
    cth::co::scheduler _workers;
    // destroyed after the queues, before the workers and the cvk core: it posts to the workers
    std::optional<TimelineReactor> _reactorOpt{};
    // destroyed before the reactor, the workers and the cvk core
    std::optional<Queues> _queuesOpt{};

public:
    Core(Core const& other) = delete;
    Core(Core&& other) = delete;
    Core& operator=(Core const& other) = delete;
    Core& operator=(Core&& other) = delete;

    static constexpr void debug_check(Core const&);
};
}

namespace jvk {
constexpr void Core::debug_check(Core const& core) {
    CTH_CRITICAL(!core.created(), "core must be created") {}
}
}
