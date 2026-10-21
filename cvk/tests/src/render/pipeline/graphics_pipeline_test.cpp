#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/pipeline/graphics_pipeline.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"

#include <array>
#include <memory>


namespace cvk {
namespace {
    constexpr std::array COLOR_FORMATS{VK_FORMAT_B8G8R8A8_UNORM};
    constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

    /**
     * @brief layout and shaders of the triangle shaders
     */
    struct Triangle {
        explicit Triangle(Core const& core) : setLayout{core, set_config(), create},
            layout{
                core,
                PipelineLayout::Config{
                    .setLayouts = {&setLayout},
                    .pushConstantRanges = {{.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = 16}},
                },
                create
            },
            vertex{core, VK_SHADER_STAGE_VERTEX_BIT, test::shaders::TRIANGLE_VERT},
            fragment{core, VK_SHADER_STAGE_FRAGMENT_BIT, test::shaders::TRIANGLE_FRAG} {}

        [[nodiscard]] GraphicsPipeline::Config config() const {
            GraphicsPipeline::Config config{COLOR_FORMATS, DEPTH_FORMAT};
            config.addStage(vertex).addStage(fragment);
            config.vertexBindings = {
                {.binding = 0, .stride = 3 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
            };
            config.vertexAttributes = {
                {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
            };
            return config;
        }

        static DescriptorSetLayout::Config set_config() {
            DescriptorSetLayout::Config config{};
            config.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
            return config;
        }

        DescriptorSetLayout setLayout;
        PipelineLayout layout;
        Shader vertex;
        Shader fragment;
    };
}

CVK_TEST(GraphicsPipeline, config_from_the_formats) {
    GraphicsPipeline::Config const config{COLOR_FORMATS, DEPTH_FORMAT};

    EXPECT_TRUE(std::ranges::equal(config.colorFormats, COLOR_FORMATS));
    ASSERT_EQ(config.colorBlendAttachments.size(), 1);
    EXPECT_EQ(config.colorBlendAttachments[0].blendEnable, VK_TRUE);
    EXPECT_EQ(config.depthFormat, DEPTH_FORMAT);
}

CVK_TEST(GraphicsPipeline, config_is_constexpr) {
    static_assert([] {
        GraphicsPipeline::Config config{COLOR_FORMATS, DEPTH_FORMAT};
        config.stages.push_back(
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = VK_NULL_HANDLE,
                .pName = "main",
                .pSpecializationInfo = nullptr,
            }
        );
        config.removeStage(VK_SHADER_STAGE_VERTEX_BIT);
        return config.colorBlendAttachments.size() == 1 && config.stages.empty();
    }());
}

CVK_TEST(GraphicsPipeline, config_stages) {
    test::Context const context{};
    Triangle const triangle{context.core};

    auto config = triangle.config();
    ASSERT_EQ(config.stages.size(), 2);
    EXPECT_EQ(config.stages[0].module, triangle.vertex.get());

    config.removeStage(VK_SHADER_STAGE_VERTEX_BIT);
    ASSERT_EQ(config.stages.size(), 1);
    EXPECT_EQ(config.stages[0].stage, VK_SHADER_STAGE_FRAGMENT_BIT);
}

CVK_TEST(GraphicsPipeline, bind) {
    test::Context const context{};
    Triangle const triangle{context.core};
    GraphicsPipeline const pipeline{triangle.layout, triangle.config(), create};
    CmdPool const pool{context.core, CmdPool::Config{context.core.queue(0)}, create};
    PrimaryCmdBuffer primary{pool, create};

    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    primary.bindPipeline(pipeline);
    primary.end();

    test::submit_and_wait(context, primary);
}

CVK_TEST(GraphicsPipeline, base_constructor_does_not_create) {
    test::Context const context{};
    Triangle const triangle{context.core};

    GraphicsPipeline const pipeline{triangle.layout, triangle.config()};

    EXPECT_FALSE(pipeline.created());
    EXPECT_EQ(&pipeline.layout(), &triangle.layout);
    EXPECT_EQ(pipeline.bindPoint(), VK_PIPELINE_BIND_POINT_GRAPHICS);
}

CVK_TEST(GraphicsPipeline, create) {
    test::Context const context{};
    Triangle const triangle{context.core};

    GraphicsPipeline const pipeline{triangle.layout, triangle.config(), create};

    EXPECT_TRUE(pipeline.created());
    EXPECT_EQ(pipeline.config().stages.size(), 2);
}

CVK_TEST(GraphicsPipeline, create_without_depth) {
    test::Context const context{};
    Triangle const triangle{context.core};

    auto config = triangle.config();
    config.depthFormat = VK_FORMAT_UNDEFINED;

    GraphicsPipeline const pipeline{triangle.layout, std::move(config), create};

    EXPECT_TRUE(pipeline.created());
}

CVK_TEST(GraphicsPipeline, shaders_can_be_destroyed_after_create) {
    test::Context const context{};
    auto triangle = std::make_unique<Triangle>(context.core);

    GraphicsPipeline pipeline{triangle->layout, triangle->config(), create};
    triangle->vertex.destroy();
    triangle->fragment.destroy();

    EXPECT_TRUE(pipeline.created());
}

CVK_TEST(GraphicsPipeline, release_and_wrap) {
    test::Context const context{};
    Triangle const triangle{context.core};
    GraphicsPipeline pipeline{triangle.layout, triangle.config(), create};
    auto const vkPipeline = pipeline.get();

    auto const state = pipeline.release();
    EXPECT_FALSE(pipeline.created());

    GraphicsPipeline const wrapped{triangle.layout, triangle.config(), state};
    EXPECT_EQ(wrapped.get(), vkPipeline);
}

CVK_TEST(GraphicsPipeline, move_assignment_destroys_the_old_pipeline) {
    test::Context const context{};
    Triangle const triangle{context.core};
    GraphicsPipeline pipeline{triangle.layout, triangle.config(), create};
    GraphicsPipeline other{triangle.layout, triangle.config(), create};
    auto const vkPipeline = other.get();

    pipeline = std::move(other);

    EXPECT_EQ(pipeline.get(), vkPipeline);
    EXPECT_FALSE(other.created());
}

CVK_TEST(GraphicsPipeline, derivative) {
    test::Context const context{};
    Triangle const triangle{context.core};

    auto parentConfig = triangle.config();
    parentConfig.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    GraphicsPipeline const parent{triangle.layout, std::move(parentConfig), create};

    auto childConfig = triangle.config();
    childConfig.rasterization.cullMode = VK_CULL_MODE_NONE;
    GraphicsPipeline const child{parent, std::move(childConfig), create};

    ASSERT_TRUE(child.created());
    EXPECT_EQ(child.parent(), parent.get());
    EXPECT_EQ(&child.layout(), &triangle.layout);
    EXPECT_EQ(parent.parent(), VK_NULL_HANDLE);
}
}
