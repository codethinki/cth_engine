#pragma once
#include "test.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/instance.hpp"


namespace cvk::test {
/**
 * @brief instance and core for tests, fails the test on validation errors
 * @details members destroy in reverse order -> the check sees the errors of vkDestroyInstance() too, e.g. leaks
 */
struct Context {
    /**
     * @brief one queue, some devices have one queue only
     */
    static Core::Config const CONFIG;

    explicit Context(Core::Config const& config = CONFIG);

    struct ValidationCheck {
        ValidationCheck();
        ~ValidationCheck();
    } validationCheck;

    Instance instance;
    Core core;

    Context(Context const& other) = delete;
    Context(Context&& other) = delete;
    Context& operator=(Context const& other) = delete;
    Context& operator=(Context&& other) = delete;
};
}
