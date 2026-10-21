#pragma once
#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"

#include <array>
#include <memory>
#include <utility>


namespace cvk::test {
/**
 * @brief instance and core for tests, fails the test on validation errors
 * @details the default is the shared instance and core of the process, made at the first use,
 *  @ref destroy_shared_contexts() destroys them, the leaks of all tests show up there
 */
struct Context {
    /**
     * @brief one queue, some devices have one queue only
     */
    static Core::Config const CONFIG;

    struct ValidationCheck {
        ValidationCheck();
        ~ValidationCheck();
    };

    /**
     * @brief instance and core that a context owns
     */
    struct Owned;

    /**
     * @brief the shared instance and core
     */
    Context();

    /**
     * @brief own instance and core with the config
     * @details destroys them at the end -> the check sees the errors of vkDestroyInstance() too, e.g. leaks
     */
    explicit Context(Core::Config const&);

    ~Context();

    ValidationCheck validationCheck;

private:
    std::unique_ptr<Owned> _owned;

public:
    Instance const& instance;
    Core const& core;

    Context(Context const& other) = delete;
    Context(Context&& other) = delete;
    Context& operator=(Context const& other) = delete;
    Context& operator=(Context&& other) = delete;
};

/**
 * @brief instance, headless surface and core with a present queue, fails the test on validation errors
 * @details the shared ones of the process, made at the first use, see @ref Context
 */
struct PresentContext {
    /**
     * @brief one graphics queue that presents
     */
    static Core::Config const CONFIG;

    PresentContext();

    Context::ValidationCheck validationCheck;

    Instance const& instance;
    Core const& core;

    [[nodiscard]] Surface const& surface() const { return _surface; }
    [[nodiscard]] Queue const& queue() const { return core.queue(0); }

private:
    Surface const& _surface;

public:

    PresentContext(PresentContext const& other) = delete;
    PresentContext(PresentContext&& other) = delete;
    PresentContext& operator=(PresentContext const& other) = delete;
    PresentContext& operator=(PresentContext&& other) = delete;
};

/**
 * @brief destroys the shared contexts, fails on their validation errors
 * @details the test environment calls it before the loader unloads
 */
void destroy_shared_contexts();

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
 * @param layout of the mip level, a transfer src layout
 * @pre the image has a color aspect
 */
void copy_to_buffer(
    Context const&,
    BaseCmdBuffer const&,
    Image const&,
    BaseBuffer const& dst,
    size_t dstOffset,
    uint32_t mipLevel,
    VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
);

/**
 * @brief records into a primary command buffer of a new pool, submits it and waits for the gpu
 * @param record called with the recording PrimaryCmdBuffer&
 */
template<class Record>
void record_and_submit(Context const& context, Record&& record) {
    CmdPool const pool{context.core, CmdPool::Config{.queueFamilyIndex = context.core.queue(0).familyIndex()}, create};
    PrimaryCmdBuffer primary{pool, create};

    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    std::forward<Record>(record)(primary);
    primary.end();

    submit_and_wait(context, primary);
}
}
