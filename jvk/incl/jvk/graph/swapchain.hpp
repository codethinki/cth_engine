#pragma once
#include "jvk/graph/presenter.hpp"

#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>


namespace cvk {
class Core;
class Surface;
}

namespace jvk {
class Queue;
}

namespace jvk::dev {
/**
 * swapchain with the presenter of its images
 * @details the presenter refers to the swapchain: the pair does not move
 */
struct PresentedSwapchain {
    /**
     * creates the swapchain, then its presenter with `framesInFlight` acquire semaphores
     * @details see `cvk::Swapchain::create()` and `Presenter()`
     */
    PresentedSwapchain(
        cvk::Core const& core,
        Queue& presentQueue,
        cvk::Surface const& surface,
        cvk::Swapchain::Config config,
        VkExtent2D extent,
        cvk::Swapchain const* oldSwapchainPtr,
        uint32_t framesInFlight
    );

    cvk::Swapchain swapchain;
    Presenter presenter; ///< after the swapchain: destroyed before it

    PresentedSwapchain(PresentedSwapchain const& other) = delete;
    PresentedSwapchain(PresentedSwapchain&& other) = delete;
    PresentedSwapchain& operator=(PresentedSwapchain const& other) = delete;
    PresentedSwapchain& operator=(PresentedSwapchain&& other) = delete;
};
}

namespace jvk {
/**
 * swapchain with the swapchains it replaced, each with the `Presenter` of its images
 * @details
 * - holds the current `cvk::Swapchain` and the retired ones, at stable addresses
 * - `recreate()` creates the next swapchain with the current one as the old swapchain, the old one
 *   retires
 * - only the presenter of the current swapchain acquires, a retired one presents and drops the
 *   images it handed out
 * - `acquire()` destroys the retired swapchains whose presenter finished first
 * @attention not thread safe: the surface orders its uses, see `Surface`
 */
class Swapchain {
public:
    using Config = cvk::Swapchain::Config;

    /**
     * base constructor
     * @details the presenters present through the queue, each with `framesInFlight` acquire
     *  semaphores
     * @pre `framesInFlight > 0`
     */
    Swapchain(
        cvk::Core const&,
        Queue& presentQueue,
        cvk::Surface const&,
        uint32_t framesInFlight
    );

    /**
     * @note calls `optDestroy()`
     */
    ~Swapchain() { optDestroy(); }

    /**
     * creates the first swapchain and its presenter
     * @details see `cvk::Swapchain::create()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of `vkCreateSwapchainKHR()` or
     *  `vkGetSwapchainImagesKHR()`
     * @throws cvk::vk_result_exception the exceptions of `Presenter()`
     */
    void create(Config, VkExtent2D extent);

    /**
     * creates the next swapchain and its presenter, the current one as the old swapchain retires
     * @details with the config of the current swapchain, see `cvk::Swapchain::create()`
     * @pre `created()`
     * @pre the preconditions of `cvk::Swapchain::create()`
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode, the
     *  current swapchain stays
     * @throws cvk::vk_result_exception result of `vkCreateSwapchainKHR()` or
     *  `vkGetSwapchainImagesKHR()` or the exceptions of `Presenter()`: the current swapchain stays,
     *  retired, `acquire()` returns `std::nullopt` until the next `recreate()`
     */
    void recreate(VkExtent2D extent);

    /**
     * destroys every presenter and its swapchain and forgets
     * @details
     * - the oldest first, the presenter before its swapchain
     * - waits for the pending present fences first, see `Presenter::waitPresents()`
     * - a failed wait does not stop the destruction, e.g. `VK_ERROR_DEVICE_LOST`
     * @pre `created()`
     * @pre no `Presenter::present()` runs
     * @pre the preconditions of `~Presenter()` for every presenter
     * @pre the submits that signal the present semaphores completed, see
     *  `cvk::Swapchain::destroy()`
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
     * destroys the finished retired swapchains, then acquires the next image of the current one
     * @details finished: `Presenter::finished()`, from the oldest on: no present fence is pending
     * @details the current swapchain retired by a failed `recreate()` -> `std::nullopt`
     * @return the image, `std::nullopt` -> out of date, `recreate()` the swapchain
     * @pre `created()`
     * @see `Presenter::acquire()`
     */
    [[nodiscard]] cth::co::executor_task<std::optional<Presenter::AcquiredImage>> acquire();

    /**
     * @return the swapchain of the next acquires, its address stays until its destruction
     * @pre `created()`
     */
    [[nodiscard]] cvk::Swapchain& current() {
        debug_check(*this);
        return _swapchains.back().swapchain;
    }

    /**
     * @pre `created()`
     */
    [[nodiscard]] cvk::Swapchain const& current() const {
        debug_check(*this);
        return _swapchains.back().swapchain;
    }

    /**
     * @return the presenter of the current swapchain
     * @pre `created()`
     */
    [[nodiscard]] Presenter& presenter() {
        debug_check(*this);
        return _swapchains.back().presenter;
    }

    [[nodiscard]] bool created() const { return !_swapchains.empty(); }

    /**
     * @return count of the retired swapchains that are not destroyed yet
     */
    [[nodiscard]] size_t retired() const { return created() ? _swapchains.size() - 1 : 0; }

private:
    cth::ref_wrapper<cvk::Core const> _core;
    cth::ref_wrapper<Queue> _presentQueue;
    cth::ref_wrapper<cvk::Surface const> _surface;
    uint32_t _framesInFlight;

    /**
     * `back()` is the current swapchain, the others are retired, the oldest first
     * @details a deque: the addresses stay at the end
     */
    std::deque<dev::PresentedSwapchain> _swapchains{};

    /** a failed `recreate()` retired the current swapchain */
    bool _currentRetired = false;

public:
    Swapchain(Swapchain const& other) = delete;
    Swapchain(Swapchain&& other) noexcept = default;
    Swapchain& operator=(Swapchain const& other) = delete;
    Swapchain& operator=(Swapchain&& other) noexcept = default;

    static void debug_check(Swapchain const&);
};
}

namespace jvk {
inline void Swapchain::debug_check(Swapchain const& swapchain) {
    CTH_CRITICAL(!swapchain.created(), "swapchain must be created") {}
}
}
