#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/data/buffer/buffer.hpp"
#include "cvk/data/descriptor/descriptor_pool.hpp"
#include "cvk/data/descriptor/descriptor_set.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/pipeline/compute_pipeline.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <array>
#include <cstdint>


namespace cvk {
namespace {
    /**
     * @brief work groups of the values shader, 64 invocations each
     */
    constexpr uint32_t GROUPS = 4;
    constexpr uint32_t VALUES = GROUPS * 64;

    constexpr Buffer<uint32_t>::Config OUTPUT_CONFIG{
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT},
    };

    /**
     * @brief set layout, pipeline layout and shader of the values shader
     */
    struct Values {
        explicit Values(Core const& core) : setLayout{core, set_config(), create},
            layout{core, PipelineLayout::Config{.setLayouts = {&setLayout}}, create},
            shader{core, VK_SHADER_STAGE_COMPUTE_BIT, test::shaders::VALUES_COMP} {}

        [[nodiscard]] ComputePipeline::Config config() const { return {.stage = shader.stageInfo()}; }

        static DescriptorSetLayout::Config set_config() {
            DescriptorSetLayout::Config config{};
            config.add(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
            return config;
        }

        DescriptorSetLayout setLayout;
        PipelineLayout layout;
        Shader shader;
    };

    /**
     * @brief binds the values pipeline and its storage buffer, records the dispatch and checks the values
     * @param dispatch records the dispatch with the command buffer
     */
    void expect_values(test::Context const& context, auto&& dispatch) {
        Values const values{context.core};
        ComputePipeline const pipeline{values.layout, values.config(), create};
        Buffer<uint32_t> const output{context.core, OUTPUT_CONFIG, VALUES};
        DescriptorPool const pool{context.core, DescriptorPool::Config::ForLayouts({{&values.setLayout}}), create};
        DescriptorSet const set{pool, values.setLayout, create};
        set.write({0, output});

        test::record_and_submit(context, [&](PrimaryCmdBuffer const& cmd) {
            cmd.bindPipeline(pipeline);
            cmd.bindDescriptorSet(set, values.layout, 0, VK_PIPELINE_BIND_POINT_COMPUTE);
            dispatch(cmd);

            Barrier toHost{};
            toHost.add(
                {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT},
                {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT}
            );
            toHost.execute(cmd);
        });

        auto const mapped = output.map();
        output.invalidate();
        for(uint32_t i = 0; i < VALUES; ++i)
            EXPECT_EQ(mapped[i], 3 * i);
    }
}

CVK_TEST(ComputePipeline, base_constructor_does_not_create) {
    test::Context const context{};
    Values const values{context.core};

    ComputePipeline const pipeline{values.layout, values.config()};

    EXPECT_FALSE(pipeline.created());
    EXPECT_EQ(&pipeline.layout(), &values.layout);
    EXPECT_EQ(pipeline.bindPoint(), VK_PIPELINE_BIND_POINT_COMPUTE);
}

CVK_TEST(ComputePipeline, create_and_destroy) {
    test::Context const context{};
    Values const values{context.core};

    ComputePipeline pipeline{values.layout, values.config(), create};
    ASSERT_TRUE(pipeline.created());
    EXPECT_EQ(&pipeline.core(), &context.core);

    pipeline.destroy();
    EXPECT_FALSE(pipeline.created());
}

CVK_TEST(ComputePipeline, release_and_wrap) {
    test::Context const context{};
    Values const values{context.core};
    ComputePipeline pipeline{values.layout, values.config(), create};
    auto const vkPipeline = pipeline.get();

    auto const state = pipeline.release();
    EXPECT_FALSE(pipeline.created());

    ComputePipeline const wrapped{values.layout, values.config(), state};
    EXPECT_EQ(wrapped.get(), vkPipeline);
}

CVK_TEST(ComputePipeline, move_assignment_destroys_the_old_pipeline) {
    test::Context const context{};
    Values const values{context.core};
    ComputePipeline pipeline{values.layout, values.config(), create};
    ComputePipeline other{values.layout, values.config(), create};
    auto const vkPipeline = other.get();

    pipeline = std::move(other);

    EXPECT_EQ(pipeline.get(), vkPipeline);
    EXPECT_FALSE(other.created());
}

CVK_TEST(ComputePipeline, dispatch_fills_a_storage_buffer) {
    test::Context const context{};

    expect_values(context, [](BaseCmdBuffer const& cmd) { cmd.dispatch(GROUPS); });
}

CVK_TEST(ComputePipeline, dispatch_indirect_fills_a_storage_buffer) {
    test::Context const context{};
    Buffer<VkDispatchIndirectCommand> const indirect{
        context.core,
        {
            .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
        },
        1
    };
    indirect.write(std::array{VkDispatchIndirectCommand{GROUPS, 1, 1}});

    expect_values(context, [&](BaseCmdBuffer const& cmd) { cmd.dispatchIndirect(indirect); });
}
}
