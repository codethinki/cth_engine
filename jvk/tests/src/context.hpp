#pragma once
#include "test.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/debug/debug_messenger.hpp"

#include <array>


namespace jvk::test {
/**
 * one queue, some devices have one queue only
 */
inline constexpr std::array QUEUES{cvk::QueueFamilyProperties::GRAPHICS | cvk::QueueFamilyProperties::COMPUTE};

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
     * the queues of @ref QUEUES
     */
    static cvk::Core::Config const CONFIG;

    struct ValidationCheck {
        ValidationCheck();
        ~ValidationCheck();
    };

    Context();

    ValidationCheck validationCheck;

    cvk::Instance const& instance;
    cvk::Core const& core;

    Context(Context const& other) = delete;
    Context(Context&& other) = delete;
    Context& operator=(Context const& other) = delete;
    Context& operator=(Context&& other) = delete;
};

/**
 * destroys the shared contexts, fails on their validation errors
 * @details the test environment calls it before the loader unloads
 */
void destroy_shared_contexts();
}
