#pragma once
#include "test.hpp"

#include "jvk/base/core.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/debug/debug_messenger.hpp"


namespace timgui::test {
/**
 * messenger config that logs every message and counts the errors
 * @details a small copy of the cvk tests, the instance create info gets it too:
 *  counts the errors of `vkDestroyInstance()`, e.g. leaked objects
 */
[[nodiscard]] cvk::DebugMessenger::Config counting_messenger();

/**
 * own instance and core, fails the test on validation errors
 * @details the check runs after the instance is destroyed
 */
struct Context {
    /**
     * one graphics queue, the heaps of `jvk::HeapsConfig`
     */
    static jvk::Core::Config const CONFIG;

    /**
     * resets the error count, the destructor fails the test on errors
     */
    struct ValidationCheck {
        ValidationCheck();
        ~ValidationCheck();
    };

    Context();

    ValidationCheck validationCheck;
    cvk::Instance instance;
    jvk::Core core;

    Context(Context const& other) = delete;
    Context(Context&& other) = delete;
    Context& operator=(Context const& other) = delete;
    Context& operator=(Context&& other) = delete;
};
}
