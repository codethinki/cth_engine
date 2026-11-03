#pragma once
#include "test.hpp"

#include "jvk/base/core.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/debug/debug_messenger.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"

#include <array>


namespace jvk::test {
/**
 * one queue, some devices have one queue only
 */
inline constexpr std::array QUEUES{cvk::QueueFamilyProperties::GRAPHICS | cvk::QueueFamilyProperties::COMPUTE};

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
 *  counts the errors of vkDestroyInstance(), e.g. leaked objects
 */
[[nodiscard]] cvk::DebugMessenger::Config counting_messenger();

/**
 * shared instance and core for tests, fails the test on validation errors
 * @details made at the first use, @ref destroy_shared_contexts() destroys them, the leaks of all tests show up there
 */
struct Context {
    /**
     * the queues of @ref QUEUES, two workers and two queue workers: jobs of one strand can run on different threads
     */
    static Core::Config const CONFIG;

    struct ValidationCheck {
        ValidationCheck();
        ~ValidationCheck();
    };

    Context();

    ValidationCheck validationCheck;

    cvk::Instance const& instance;
    Core const& core;

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
 * destroys the shared contexts, fails on their validation errors
 * @details the test environment calls it before the loader unloads
 */
void destroy_shared_contexts();
}
