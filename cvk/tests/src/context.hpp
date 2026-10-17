#pragma once
#include "test.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
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
 * @brief records a copy of the mip level into the buffer, without cvk: tests need a second path
 * @pre the mip level is in a transfer src layout, the image has a color aspect
 */
void copy_to_buffer(
    Context const&,
    BaseCmdBuffer const&,
    Image const&,
    BaseBuffer const& dst,
    size_t dstOffset,
    uint32_t mipLevel
);

/**
 * @brief records into a primary command buffer of a new pool, submits it and waits for the gpu
 * @param record called with the recording PrimaryCmdBuffer&
 */
template<class Record>
void record_and_submit(Context const& context, Record&& record) {
    CmdPool const pool{context.core, CmdPool::Config::Default(context.core.queue(0)), create};
    PrimaryCmdBuffer primary{pool, create};

    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    std::forward<Record>(record)(primary);
    primary.end();

    submit_and_wait(context, primary);
}
}
