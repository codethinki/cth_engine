#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/data/buffer/buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/pipeline/compute_pipeline.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <array>
#include <cstdint>


namespace cvk {
namespace {
    /**
     * work groups of the values shader, 64 invocations each
     */
    constexpr uint32_t GROUPS = 4;
    constexpr uint32_t VALUES = GROUPS * 64;

    constexpr Buffer<uint32_t>::Config OUTPUT_CONFIG{
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT},
    };

    /**
     * shader of the values shader
     */
    struct Values {
        explicit Values(Core const& core) : shader{
            core,
            VK_SHADER_STAGE_COMPUTE_BIT,
            test::shaders::VALUES_COMP
        } {}

        [[nodiscard]] ComputePipeline::Config config() const {
            return {.stage = shader.stageInfo()};
        }

        Shader shader;
    };

    /**
     * binds the values pipeline, pushes the address of its output, records the dispatch and checks the values
     * @param dispatch records the dispatch with the command buffer
     */
    void expect_values(test::Context const& context, auto&& dispatch) {
        Values const values{context.core};
        ComputePipeline const pipeline{context.core, values.config(), create};
        Buffer<uint32_t> const output{context.core, OUTPUT_CONFIG, VALUES};

        test::record_and_submit(
            context,
            [&](PrimaryCmdBuffer const& cmd) {
                cmd.bind(pipeline);
                cmd.pushData(output.address());
                dispatch(cmd);

                Barrier toHost{};
                toHost.add(
                    {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT},
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT}
                );
                toHost.execute(cmd);
            }
        );

        auto const mapped = output.map();
        output.invalidate();
        for(uint32_t i = 0; i < VALUES; ++i)
            EXPECT_EQ(mapped[i], 3 * i);
    }
}

CVK_TEST(ComputePipeline, base_constructor_does_not_create) {
    test::Context const context{};
    Values const values{context.core};

    ComputePipeline const pipeline{context.core, values.config()};

    EXPECT_FALSE(pipeline.created());
    EXPECT_EQ(&pipeline.core(), &context.core);
    EXPECT_EQ(pipeline.bindPoint(), VK_PIPELINE_BIND_POINT_COMPUTE);
}

CVK_TEST(ComputePipeline, create_and_destroy) {
    test::Context const context{};
    Values const values{context.core};

    ComputePipeline pipeline{context.core, values.config(), create};
    ASSERT_TRUE(pipeline.created());
    EXPECT_EQ(&pipeline.core(), &context.core);

    pipeline.destroy();
    EXPECT_FALSE(pipeline.created());
}

CVK_TEST(ComputePipeline, release_and_wrap) {
    test::Context const context{};
    Values const values{context.core};
    ComputePipeline pipeline{context.core, values.config(), create};
    auto const vkPipeline = pipeline.get();

    auto const state = pipeline.release();
    EXPECT_FALSE(pipeline.created());

    ComputePipeline const wrapped{context.core, values.config(), state};
    EXPECT_EQ(wrapped.get(), vkPipeline);
}

CVK_TEST(ComputePipeline, move_assignment_destroys_the_old_pipeline) {
    test::Context const context{};
    Values const values{context.core};
    ComputePipeline pipeline{context.core, values.config(), create};
    ComputePipeline other{context.core, values.config(), create};
    auto const vkPipeline = other.get();

    pipeline = std::move(other);

    EXPECT_EQ(pipeline.get(), vkPipeline);
    EXPECT_FALSE(other.created());
}

CVK_TEST(ComputePipeline, dispatch_fills_a_buffer_at_the_pushed_address) {
    test::Context const context{};

    expect_values(context, [](BaseCmdBuffer const& cmd) { cmd.dispatch(GROUPS); });
}

CVK_TEST(ComputePipeline, dispatch_indirect_fills_a_buffer_at_the_pushed_address) {
    test::Context const context{};
    Buffer<VkDispatchIndirectCommand> const indirect{
        context.core,
        test::host_written(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        1
    };
    indirect.write(std::array{VkDispatchIndirectCommand{GROUPS, 1, 1}});

    expect_values(context, [&](BaseCmdBuffer const& cmd) { cmd.dispatchIndirect(indirect); });
}

CVK_TEST(ComputePipeline, decompose_returns_the_released_handle) {
    // a leak shows up when the shared device is destroyed
    test::Context const context{};
    Values const values{context.core};
    ComputePipeline pipeline{context.core, values.config(), create};
    auto const vkPipeline = pipeline.get();

    auto const [vkReleased] = pipeline.release().decompose();
    EXPECT_EQ(vkReleased, vkPipeline);

    cvk::destroy(context.core.deviceTable(), vkReleased);
}
}
