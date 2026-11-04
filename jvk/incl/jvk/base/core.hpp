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
 * jvk core
 * @details owns the cvk core, the workers and the serialized queues
 * @details roles with the same vulkan queue share one @ref Queue
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
     * @details calls @ref Core(cvk::Instance const&, Config), @ref create()
     */
    explicit Core(cvk::Instance const&, Config, cvk::create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~Core();

    /**
     * creates the cvk core, the workers and the queues
     * @details enables @ref cvk::constants::OS_FENCE_DEVICE_EXTENSIONS in addition to the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     * @throws cvk::cvk_exception reason: @ref cvk::CoreConfig::device is missing or does not meet the requirements
     * @throws cvk::vk_result_exception result of vkCreateDevice()
     * @throws cvk::vk_result_exception result of vmaCreateAllocator()
     */
    void create();

    /**
     * destroys the queues, the workers and the cvk core
     * @details waits until every queue is idle first
     * @details a failed wait does not stop the destruction
     * @pre @ref created()
     * @pre the calling thread runs no job of the core
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
     * the cvk core
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
