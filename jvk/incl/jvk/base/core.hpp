#pragma once
#include "core_config.hpp"
#include "queues.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/constants.hpp"

#include <cth/coro/executor.hpp>
#include <cth/coro/scheduler.hpp>
#include <cth/io/log.hpp>

#include <cstddef>
#include <optional>


namespace cvk {
class Instance;
}

namespace jvk {
/**
 * cvk core with workers and serialized queues, neither copyable nor movable
 * @details one @ref Queue per vulkan queue, roles with the same vulkan queue share one @ref Queue,
 *  converts to cvk::Core const& for the cvk objects
 */
class Core {
public:
    using Config = CoreConfig;

    /**
     * base constructor
     * @pre @ref cvk::Instance::created()
     */
    Core(cvk::Instance const&, Config);

    /**
     * constructs and creates
     * @details calls:
     * - @ref Core(cvk::Instance const&, Config)
     * - @ref create()
     */
    explicit Core(cvk::Instance const&, Config, cvk::create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~Core();

    /**
     * creates the cvk core, starts the workers, constructs the @ref Queues
     * @details the cvk core enables @ref cvk::constants::OS_FENCE_DEVICE_EXTENSIONS on top of the config,
     *  empty @ref cvk::CoreConfig::queueSets -> @ref queue_sets() of @ref cvk::CoreConfig::queueProperties
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     * @throws cvk::cvk_exception reason: @ref cvk::CoreConfig::device is missing or does not meet the requirements
     * @throws cvk::vk_result_exception result of vkCreateDevice()
     * @throws cvk::vk_result_exception result of vmaCreateAllocator()
     */
    void create();

    /**
     * waits until every queue is idle, destroys the @ref Queues, stops the workers, destroys the cvk core
     * @pre @ref created()
     * @pre the calling thread runs no job of the core
     * @throws cvk::vk_result_exception result of vkQueueWaitIdle()
     */
    void destroy();

    /**
     * if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @param role index in @ref cvk::CoreConfig::queueProperties
     * @return serialized queue of the role, roles with the same vulkan queue return the same object
     * @pre @ref created()
     * @pre role < size of @ref cvk::CoreConfig::queueProperties
     */
    [[nodiscard]] Queue const& queue(size_t role) const;

    /**
     * @return executor of the workers
     * @pre @ref created()
     */
    [[nodiscard]] constexpr cth::co::executor executor() const {
        debug_check(*this);
        return cth::co::executor{_workers};
    }

    /**
     * the cvk core, for the cvk objects
     * @attention cvk::Core::queue() returns the queue without its strand
     */
    constexpr operator cvk::Core const&() const { return _core; }

    [[nodiscard]] constexpr bool created() const { return _core.created(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    Config _config;
    cvk::Core _core;
    cth::co::scheduler _workers;
    // destroyed before the workers and the cvk core
    std::optional<Queues> _queues{};

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
