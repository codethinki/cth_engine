#pragma once
#include "cvk/surface/swapchain.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>


namespace cvk {
class Core;
class Queue;
class Semaphore;
class Surface;
}

namespace jvk {
/**
 * swapchain with the swapchains it replaced
 * @details
 * - holds the current `cvk::Swapchain` and the retired ones, at stable addresses
 * - `recreate()` creates the next swapchain with the current one as the old swapchain, the old one retires
 * - a retired swapchain still presents its acquired images
 * - `acquire()` destroys the retired swapchains that finished their presents first
 * @attention not thread safe: the surface orders its uses, see `Surface`
 */
class Swapchain {
public:
    using Config = cvk::Swapchain::Config;

    /**
     * base constructor
     * @details the swapchains present through the queue
     */
    Swapchain(cvk::Core const&, cvk::Queue const& presentQueue, cvk::Surface const&);

    /**
     * @note calls `optDestroy()`
     */
    ~Swapchain() { optDestroy(); }

    /**
     * creates the first swapchain
     * @details see `cvk::Swapchain::create()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of `vkCreateSwapchainKHR()` or `vkGetSwapchainImagesKHR()`
     */
    void create(Config, VkExtent2D extent);

    /**
     * creates the next swapchain with the current one as the old swapchain, the old one retires
     * @details with the config of the current swapchain, see `cvk::Swapchain::create()`
     * @pre `created()`
     * @pre the preconditions of `cvk::Swapchain::create()`
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode, the current swapchain
     *  stays
     * @throws cvk::vk_result_exception result of `vkCreateSwapchainKHR()` or `vkGetSwapchainImagesKHR()`, the
     *  current swapchain stays retired: its acquires fail with `VK_ERROR_OUT_OF_DATE_KHR`
     */
    void recreate(VkExtent2D extent);

    /**
     * destroys every swapchain and forgets
     * @pre `created()`
     * @pre the preconditions of `cvk::Swapchain::destroy()` for every swapchain
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
     * destroys the finished retired swapchains, then acquires the next image of the current swapchain
     * @details finished: `cvk::Swapchain::presented()` and not `cvk::Swapchain::acquired()`, from the oldest on
     * @pre `created()`
     * @see `cvk::Swapchain::acquire()`
     */
    [[nodiscard]] cvk::Swapchain::Acquired acquire(
        cvk::Semaphore const& signal,
        uint64_t timeout = std::numeric_limits<uint64_t>::max()
    );

    /**
     * @return the swapchain of the next acquires, its address stays until its destruction
     * @pre `created()`
     */
    [[nodiscard]] cvk::Swapchain& current() {
        debug_check(*this);
        return _swapchains.back();
    }

    /**
     * @pre `created()`
     */
    [[nodiscard]] cvk::Swapchain const& current() const {
        debug_check(*this);
        return _swapchains.back();
    }

    [[nodiscard]] bool created() const { return !_swapchains.empty(); }

    /**
     * @return count of the retired swapchains that are not destroyed yet
     */
    [[nodiscard]] size_t retired() const { return created() ? _swapchains.size() - 1 : 0; }

private:
    cth::ref_wrapper<cvk::Core const> _core;
    cth::ref_wrapper<cvk::Queue const> _presentQueue;
    cth::ref_wrapper<cvk::Surface const> _surface;

    /**
     * `back()` is the current swapchain, the others are retired, the oldest first
     * a deque: the addresses stay at the end
     */
    std::deque<cvk::Swapchain> _swapchains{};

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
