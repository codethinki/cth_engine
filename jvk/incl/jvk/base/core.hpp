#pragma once
#include "core_config.hpp"
#include "queue.hpp"

#include "jvk/data/resource_heap.hpp"
#include "jvk/data/sampler_heap.hpp"
#include "jvk/sync/timeline_reactor.hpp"

#include "cvk/base/core.hpp"

#include <cth/coro/executor.hpp>
#include <cth/coro/scheduler.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>


namespace cvk {
class Instance;
class Surface;
}

namespace jvk {
/**
 * jvk core
 * @details owns the cvk core, the workers, the timeline reactor, the serialized queues and the descriptor heaps
 * @details the indices of one vulkan queue share one `Queue`
 * @details a created core neither moves nor takes a move assignment: its queues, heaps, command
 *  buffers and reactor refer to it
 */
class Core {
public:
    using Config = CoreConfig;

    /**
     * base constructor
     */
    Core(cvk::Instance const&, Config);

    // no default for the surfaces, like cvk::Core::create(): a core without windows passes {}
    /**
     * constructs and creates
     * @details calls @ref Core(cvk::Instance const&, Config), `create()`
     */
    Core(
        cvk::Instance const&,
        Config,
        std::span<cth::ref_wrapper<cvk::Surface const> const> surfaces
    );

    /**
     * constructs and creates with the surfaces of a braced list
     * @see @ref Core(cvk::Instance const&, Config, std::span<cth::ref_wrapper<cvk::Surface const> const>)
     */
    Core(
        cvk::Instance const& instance,
        Config config,
        std::initializer_list<cth::ref_wrapper<cvk::Surface const>> surfaces
    ) : Core{instance, std::move(config), std::span{surfaces}} {}

    /**
     * @note calls `optDestroy()`
     */
    ~Core();

    /**
     * creates the cvk core, the workers, the reactor, the queues and the heaps
     * @details the features: `Config::vk` and `constants::DEVICE_FEATURES`
     * @details the heaps: `Config::heaps`, a heap whose counts are all `0` -> no heap
     * @param surfaces the present queues present to every surface
     * @pre `cvk::Instance::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     * @throws cvk::cvk_exception reason: `cvk::CoreConfig::device` is missing or does not meet the requirements
     * @throws cvk::vk_result_exception result of `vkCreateDevice()`
     * @throws cvk::vk_result_exception result of `vmaCreateAllocator()`
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()` for the reactor
     * @throws cvk::cvk_exception the exceptions of the heap creations
     * @see `cvk::Core::create()`
     */
    void create(std::span<cth::ref_wrapper<cvk::Surface const> const> surfaces);

    /**
     * creates with the surfaces of a braced list
     * @see @ref create(std::span<cth::ref_wrapper<cvk::Surface const> const>)
     */
    void create(std::initializer_list<cth::ref_wrapper<cvk::Surface const>> surfaces) {
        create(std::span{surfaces});
    }

    /**
     * destroys the queues, the heaps, the reactor, the workers and the cvk core
     * @details waits until every queue is idle first
     * @details a failed wait does not stop the destruction
     * @pre `created()`
     * @pre the calling thread runs no job of the core
     * @pre no handle of the heaps is left, the stage graphs of the core are destroyed
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

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
        return cth::co::executor{*_workersPtr};
    }

    /**
     * @return reactor of the core, waits for the timeline semaphores
     * @pre `created()`
     */
    [[nodiscard]] constexpr TimelineReactor const& reactor() const {
        debug_check(*this);
        return _reactor;
    }

    /**
     * @return the heap of the images, places the descriptors
     * @pre `created()`
     * @pre `HeapsConfig::createsResourceHeap()` of `Config::heaps`
     */
    [[nodiscard]] ResourceHeap const& resourceHeap() const;

    /**
     * @return the heap of the samplers, places the descriptors
     * @pre `created()`
     * @pre `HeapsConfig::createsSamplerHeap()` of `Config::heaps`
     */
    [[nodiscard]] SamplerHeap const& samplerHeap() const;

    /**
     * the cvk core
     * @attention `cvk::Core::queue()` returns the queue without its strand
     */
    constexpr operator cvk::Core const&() const { return *_corePtr; }

    // a moved-from core has no cvk core, its destructor asks
    [[nodiscard]] constexpr bool created() const {
        return _corePtr != nullptr && _corePtr->created();
    }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    /**
     * constructs the driver threads and one queue per vulkan queue
     * @pre the cvk core and the reactor are created
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()` for the release counters
     */
    void createQueues();

    /**
     * destroys the queues, then the driver threads that run their strands
     */
    void destroyQueues();

    Config _config;

    // behind a std::unique_ptr: the reactor and the heaps refer to the cvk core also before
    // create(), the moves stay = default
    std::unique_ptr<cvk::Core> _corePtr;
    std::unique_ptr<cth::co::scheduler> _workersPtr;

    // destroyed after the queues, before the workers and the cvk core: it posts the resumes of its
    // waiters to their contexts
    TimelineReactor _reactor;

    // destroyed after the queues: their strands run on it
    std::optional<cth::co::scheduler> _driverSchedulerOpt{}; ///< see `CoreConfig::driverThreads`
    // destroyed before the reactor, the workers and the cvk core
    std::vector<Queue> _queues{}; ///< one per vulkan queue
    /** index in `_queues` per index of `cvk::CoreConfig::queueProperties` */
    std::vector<size_t> _queueIndices{};

    // destroyed before the cvk core
    ResourceHeap _resourceHeap; ///< not created without counts, see `HeapsConfig`
    // the sampler heap needs the created cvk core, no heap without counts
    std::optional<SamplerHeap> _samplerHeapOpt{};

public:
    Core(Core const& other) = delete;
    Core(Core&& other) noexcept = default;
    Core& operator=(Core const& other) = delete;
    Core& operator=(Core&& other) noexcept = default;

    static constexpr void debug_check(Core const&);
};
}

namespace jvk {
constexpr void Core::debug_check(Core const& core) {
    CTH_CRITICAL(!core.created(), "core must be created") {}
}
}
