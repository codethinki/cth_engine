#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/pipeline.hpp"
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

        [[nodiscard]] Pipeline::Config config() const {
            auto config = Pipeline::Config::Default(COLOR_FORMATS, DEPTH_FORMAT);
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

CVK_TEST(Pipeline, config_default) {
    auto const config = Pipeline::Config::Default(COLOR_FORMATS, DEPTH_FORMAT);

    EXPECT_TRUE(std::ranges::equal(config.colorFormats, COLOR_FORMATS));
    ASSERT_EQ(config.colorBlendAttachments.size(), 1);
    EXPECT_EQ(config.colorBlendAttachments[0].blendEnable, VK_TRUE);
    EXPECT_EQ(config.depthFormat, DEPTH_FORMAT);
}

CVK_TEST(Pipeline, config_stages) {
    test::Context const context{};
    Triangle const triangle{context.core};

    auto config = triangle.config();
    ASSERT_EQ(config.stages.size(), 2);
    EXPECT_EQ(config.stages[0].module, triangle.vertex.get());

    config.removeStage(VK_SHADER_STAGE_VERTEX_BIT);
    ASSERT_EQ(config.stages.size(), 1);
    EXPECT_EQ(config.stages[0].stage, VK_SHADER_STAGE_FRAGMENT_BIT);
}

CVK_TEST(Pipeline, base_constructor_does_not_create) {
    test::Context const context{};
    Triangle const triangle{context.core};

    Pipeline const pipeline{context.core, triangle.layout, triangle.config()};

    EXPECT_FALSE(pipeline.created());
    EXPECT_EQ(pipeline.layout(), triangle.layout.get());
}

CVK_TEST(Pipeline, create) {
    test::Context const context{};
    Triangle const triangle{context.core};

    Pipeline const pipeline{context.core, triangle.layout, triangle.config(), create};

    EXPECT_TRUE(pipeline.created());
    EXPECT_EQ(pipeline.config().stages.size(), 2);
}

CVK_TEST(Pipeline, create_without_depth) {
    test::Context const context{};
    Triangle const triangle{context.core};

    auto config = triangle.config();
    config.depthFormat = VK_FORMAT_UNDEFINED;

    Pipeline const pipeline{context.core, triangle.layout, std::move(config), create};

    EXPECT_TRUE(pipeline.created());
}

CVK_TEST(Pipeline, shaders_can_be_destroyed_after_create) {
    test::Context const context{};
    auto triangle = std::make_unique<Triangle>(context.core);

    Pipeline pipeline{context.core, triangle->layout, triangle->config(), create};
    triangle->vertex.destroy();
    triangle->fragment.destroy();

    EXPECT_TRUE(pipeline.created());
}

CVK_TEST(Pipeline, release_and_wrap) {
    test::Context const context{};
    Triangle const triangle{context.core};
    Pipeline pipeline{context.core, triangle.layout, triangle.config(), create};
    auto const vkPipeline = pipeline.get();

    auto const state = pipeline.release();
    EXPECT_FALSE(pipeline.created());

    Pipeline const wrapped{context.core, triangle.layout, triangle.config(), state};
    EXPECT_EQ(wrapped.get(), vkPipeline);
}

CVK_TEST(Pipeline, move_assignment_destroys_the_old_pipeline) {
    test::Context const context{};
    Triangle const triangle{context.core};
    Pipeline pipeline{context.core, triangle.layout, triangle.config(), create};
    Pipeline other{context.core, triangle.layout, triangle.config(), create};
    auto const vkPipeline = other.get();

    pipeline = std::move(other);

    EXPECT_EQ(pipeline.get(), vkPipeline);
    EXPECT_FALSE(other.created());
}
}
