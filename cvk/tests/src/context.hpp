#pragma once
#include "test.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"

#include <utility>


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

/**
 * @brief records into a primary command buffer of a new pool, submits it and waits for the gpu
 * @param record called with the recording PrimaryCmdBuffer const&
 */
template<class Record>
void record_and_submit(Context const& context, Record&& record) {
    CmdPool const pool{context.core, CmdPool::Config::Default(context.core.queue(0)), create};
    PrimaryCmdBuffer primary{pool, create};

    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    std::forward<Record>(record)(std::as_const(primary));
    primary.end();

    submit_and_wait(context, primary);
}
}
