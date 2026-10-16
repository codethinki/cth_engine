#include "context.hpp"
#include "test.hpp"

#include "cvk/base/physical_device.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"


namespace cvk {
namespace {
    DescriptorSetLayout::Config uniform_config() {
        DescriptorSetLayout::Config config{};
        config.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        return config;
    }
}

CVK_TEST(PipelineLayout, create_empty) {
    test::Context const context{};

    PipelineLayout const layout{context.core, {}, create};

    EXPECT_TRUE(layout.created());
}

CVK_TEST(PipelineLayout, create_with_sets_and_push_constants) {
    test::Context const context{};
    DescriptorSetLayout const set0{context.core, uniform_config(), create};
    DescriptorSetLayout const set1{context.core, {}, create};

    PipelineLayout::Config const config{
        .setLayouts = {&set0, &set1},
        .pushConstantRanges = {{.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = 16}},
    };

    PipelineLayout const layout{context.core, config, create};

    EXPECT_TRUE(layout.created());
}

CVK_TEST(PipelineLayout, too_many_sets_throws) {
    test::Context const context{};
    DescriptorSetLayout const set{context.core, {}, create};

    auto const maxSets = context.core.physicalDevice().limits().maxBoundDescriptorSets;
    PipelineLayout::Config const config{
        .setLayouts = std::vector<not_null<DescriptorSetLayout const*>>(maxSets + 1, &set),
    };

    auto const construct = [&] { PipelineLayout const layout{context.core, config}; };

    EXPECT_THROW(construct(), cth::except::default_exception);
}

CVK_TEST(PipelineLayout, set_layouts_can_be_destroyed_after_create) {
    test::Context const context{};
    auto set = std::make_unique<DescriptorSetLayout>(context.core, uniform_config(), create);

    PipelineLayout const layout{context.core, PipelineLayout::Config{.setLayouts = {set.get()}}, create};
    set = nullptr;

    EXPECT_TRUE(layout.created());
}

CVK_TEST(PipelineLayout, release_and_wrap) {
    test::Context const context{};
    PipelineLayout layout{context.core, {}, create};
    auto const vkLayout = layout.get();

    auto const state = layout.release();
    EXPECT_FALSE(layout.created());

    PipelineLayout const wrapped{context.core, {}, state};
    EXPECT_EQ(wrapped.get(), vkLayout);
}

CVK_TEST(PipelineLayout, move_assignment_destroys_the_old_layout) {
    test::Context const context{};
    PipelineLayout layout{context.core, {}, create};
    PipelineLayout other{context.core, {}, create};
    auto const vkLayout = other.get();

    layout = std::move(other);

    EXPECT_EQ(layout.get(), vkLayout);
    EXPECT_FALSE(other.created());
}
}
