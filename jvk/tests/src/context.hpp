#pragma once
#include "test.hpp"

#include "jvk/base/core.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/debug/debug_messenger.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/surface/surface.hpp"

#include <cth/ptr/ref_wrapper.hpp>

#include <array>
#include <span>
#include <string>
#include <vector>


namespace jvk::test {
/**
 * one queue, some devices have one queue only
 */
inline constexpr std::array QUEUES{
    cvk::QueueFamilyProperties::GRAPHICS | cvk::QueueFamilyProperties::COMPUTE
};

/**
 * no surfaces for `Core`, for calls that cannot take a braced list like `std::optional::emplace()`
 */
inline constexpr std::span<cth::ref_wrapper<cvk::Surface const> const> NO_SURFACES{};

/**
 * buffer the cpu writes once and the gpu copies from
 */
inline constexpr cvk::BaseBuffer::Config STAGING_CONFIG{
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
};

/**
 * buffer the gpu copies into and the cpu reads back
 */
inline constexpr cvk::BaseBuffer::Config READBACK_CONFIG{
    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT},
};

/**
 * messenger config that logs every message and counts the errors
 * @details a small copy of the cvk tests, the instance create info gets it too:
 *  counts the errors of `vkDestroyInstance()`, e.g. leaked objects
 */
[[nodiscard]] cvk::DebugMessenger::Config counting_messenger();

/**
 * shared instance and core for tests, fails the test on validation errors
 * @details made at the first use, `destroy_shared_contexts()` destroys them, the leaks of all tests show up there
 */
struct Context {
    /**
     * the queues of `QUEUES`, two workers and two queue workers: jobs of one strand can run on different threads
     */
    static Core::Config const CONFIG;

    struct ValidationCheck {
        ValidationCheck();
        ~ValidationCheck();
    };

    Context();

    ValidationCheck validationCheck;

    cvk::Instance const& instance;
    Core& core;

    Context(Context const& other) = delete;
    Context(Context&& other) = delete;
    Context& operator=(Context const& other) = delete;
    Context& operator=(Context&& other) = delete;
};

/**
 * @return the family of queue 0 of the core
 */
[[nodiscard]] cvk::queue_family_index_t queue_family(Context const&);

/**
 * submits the primary to queue 0 from the calling thread, waits for its fence
 * @attention outside of the strand of the queue: no job of the strand runs at the same time
 */
void submit_and_wait(Context const&, cvk::PrimaryCmdBuffer const&);

/**
 * the instance extensions of the shared instance: the present extensions and `VK_EXT_headless_surface`
 */
[[nodiscard]] std::vector<std::string> headless_extensions();

/**
 * window extent of `headless_surface()` without an own callback
 */
inline constexpr VkExtent2D HEADLESS_EXTENT{64, 64};

/**
 * @param extentCallback for the undefined `currentExtent` of the surface
 * @return a headless surface of the instance
 * @pre the instance has `headless_extensions()`
 */
[[nodiscard]] cvk::Surface headless_surface(
    cvk::Instance const&,
    cvk::Surface::extent_callback_t extentCallback = [] { return HEADLESS_EXTENT; }
);

/**
 * destroys the shared contexts, fails on their validation errors
 * @details the test environment calls it before the loader unloads
 */
void destroy_shared_contexts();
}
