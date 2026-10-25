#pragma once
#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/pipeline/graphics_pipeline.hpp"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>


namespace cvk {
class Shader;
}

namespace cvk::test {
/**
 * one queue, some devices have one queue only
 */
inline constexpr std::array QUEUES{QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::COMPUTE};

/**
 * buffer the cpu writes once
 */
[[nodiscard]] constexpr BaseBuffer::Config host_written(VkBufferUsageFlags usage) {
    return {.usage = usage, .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT}};
}

/**
 * buffer the cpu writes once and the gpu copies from
 */
inline constexpr BaseBuffer::Config STAGING_CONFIG = host_written(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

/**
 * buffer the gpu copies into and the cpu reads back
 */
inline constexpr BaseBuffer::Config READBACK_CONFIG{
    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT},
};

/**
 * binding flags of a bindless descriptor array
 */
inline constexpr VkDescriptorBindingFlags BINDLESS = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

/**
 * vertex of @ref FULLSCREEN, the vec3 position at location 0 of the test vertex shaders
 */
struct Vec3 {
    float x, y, z;
};

/**
 * one triangle that covers the whole viewport
 */
inline constexpr std::array<Vec3, 3> FULLSCREEN{Vec3{-1.f, -1.f, 0.f}, Vec3{3.f, -1.f, 0.f}, Vec3{-1.f, 3.f, 0.f}};

/**
 * color of the fill draws
 */
inline constexpr std::array<float, 4> GREEN{0.f, 1.f, 0.f, 1.f};

/**
 * @ref GREEN as a VK_FORMAT_R8G8B8A8_UNORM pixel
 */
inline constexpr std::array<unsigned char, 4> GREEN_PIXEL{0, 255, 0, 255};

/**
 * pipeline config that draws @ref FULLSCREEN from vertex binding 0 without culling
 * @pre the shaders are created
 */
[[nodiscard]] GraphicsPipeline::Config fullscreen_config(
    Shader const& vertex,
    Shader const& fragment,
    std::span<VkFormat const> colorFormats
);

/**
 * @return every VK_FORMAT_R8G8B8A8_UNORM pixel of the buffer is the pixel
 * @pre the buffer is host visible, the gpu writes are visible to the host
 */
[[nodiscard]] bool every_pixel_is(BaseBuffer const& readback, std::array<unsigned char, 4> pixel);

/**
 * instance and core for tests, fails the test on validation errors
 * @details the default is the shared instance and core of the process, made at the first use,
 *  @ref destroy_shared_contexts() destroys them, the leaks of all tests show up there
 */
struct Context {
    /**
     * the queues of @ref QUEUES
     */
    static Core::Config const CONFIG;

    struct ValidationCheck {
        ValidationCheck();
        ~ValidationCheck();
    };

    /**
     * the shared instance and core
     */
    Context();

    /**
     * the shared instance and an own core with the config
     * @details destroys the core at the end -> the check sees the errors of vkDestroyDevice() too, e.g. leaks
     * @throws cvk::cvk_exception reason: no physical device meets the requirements
     */
    explicit Context(Core::Config const&);

    ValidationCheck validationCheck;

private:
    std::optional<Core> _core{};

public:
    Instance const& instance;
    Core const& core;

    Context(Context const& other) = delete;
    Context(Context&& other) = delete;
    Context& operator=(Context const& other) = delete;
    Context& operator=(Context&& other) = delete;
};

/**
 * instance, headless surface and core with a present queue, fails the test on validation errors
 * @details the shared ones of the process, made at the first use, see @ref Context
 */
struct PresentContext {
    /**
     * one graphics queue that presents
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
 * own instance with @ref counting_messenger(), fails the test on validation errors
 * @details for the tests that need an own instance, e.g. the leak reports of vkDestroyInstance():
 *  the check runs after the instance is destroyed
 */
struct InstanceContext {
    explicit InstanceContext(std::vector<std::string> extensions = {});

    Context::ValidationCheck validationCheck;
    Instance instance;

    InstanceContext(InstanceContext const& other) = delete;
    InstanceContext(InstanceContext&& other) = delete;
    InstanceContext& operator=(InstanceContext const& other) = delete;
    InstanceContext& operator=(InstanceContext&& other) = delete;
};

/**
 * pool config for the first queue of the core
 */
[[nodiscard]] CmdPool::Config pool_config(Core const&);

/**
 * destroys the shared contexts, fails on their validation errors
 * @details the test environment calls it before the loader unloads
 */
void destroy_shared_contexts();

/**
 * submits the command buffer to the first queue and waits for the gpu
 * @pre the command buffer is executable
 */
void submit_and_wait(Context const&, PrimaryCmdBuffer const&);

/**
 * records a memory barrier: transfer writes -> host reads
 * @pre the command buffer is recording
 */
void host_read_barrier(Context const&, BaseCmdBuffer const&);

/**
 * records a copy of the mip level into the buffer, without cvk: tests need a second path
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
 * records into a primary command buffer of a new pool, submits it and waits for the gpu
 * @param record called with the recording PrimaryCmdBuffer&
 */
template<class Record>
void record_and_submit(Context const& context, Record&& record) {
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};

    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    std::forward<Record>(record)(primary);
    primary.end();

    submit_and_wait(context, primary);
}
}
