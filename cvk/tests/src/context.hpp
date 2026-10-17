#pragma once
#include "test.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/instance.hpp"


namespace cvk {
class BaseCmdBuffer;
class PrimaryCmdBuffer;
}

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

    explicit Context(Core::Config const& = CONFIG);

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

/**
 * @brief submits the command buffer to the first queue and waits for the gpu
 * @pre the command buffer is executable
 */
void submit_and_wait(Context const&, PrimaryCmdBuffer const&);

/**
 * @brief records a memory barrier: transfer writes -> host reads
 * @pre the command buffer is recording
 */
void host_read_barrier(Context const&, BaseCmdBuffer const&);
}
