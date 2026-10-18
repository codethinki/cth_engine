#include "context.hpp"
#include "test.hpp"

#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"

#include <algorithm>


namespace cvk {
namespace {
    /**
     * @brief bindings 0 and 2, gap at 1
     */
    constexpr DescriptorSetLayout::Config gap_config() {
        DescriptorSetLayout::Config config{};
        config.add(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
              .add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 2);
        return config;
    }
}

CVK_TEST(DescriptorSetLayout, config_sorts_bindings) {
    auto const config = gap_config();

    ASSERT_EQ(config.bindings.size(), 2);
    EXPECT_EQ(config.bindings[0].binding, 0);
    EXPECT_EQ(config.bindings[0].descriptorCount, 2);
    EXPECT_EQ(config.bindings[1].binding, 2);
}

CVK_TEST(DescriptorSetLayout, config_remove) {
    auto config = gap_config();

    config.remove(0);

    ASSERT_EQ(config.bindings.size(), 1);
    EXPECT_EQ(config.bindings[0].binding, 2);
}

CVK_TEST(DescriptorSetLayout, config_is_constexpr) {
    static_assert([] {
        auto config = gap_config();
        config.remove(0);
        return config.bindings.size() == 1 && config.bindings[0].binding == 2;
    }());
}

CVK_TEST(DescriptorSetLayout, create_with_gaps) {
    test::Context const context{};

    DescriptorSetLayout const layout{context.core, gap_config(), create};

    ASSERT_TRUE(layout.created());
    EXPECT_EQ(layout.bindings().size(), 2);

    auto const* binding = layout.binding(2);
    ASSERT_NE(binding, nullptr);
    EXPECT_EQ(binding->descriptorType, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    EXPECT_EQ(layout.binding(1), nullptr);
    EXPECT_EQ(layout.binding(3), nullptr);
}

CVK_TEST(DescriptorSetLayout, create_empty) {
    test::Context const context{};

    DescriptorSetLayout const layout{context.core, {}, create};

    EXPECT_TRUE(layout.created());
    EXPECT_TRUE(layout.bindings().empty());
}

CVK_TEST(DescriptorSetLayout, release_and_wrap) {
    test::Context const context{};
    DescriptorSetLayout layout{context.core, gap_config(), create};
    auto const vkLayout = layout.get();

    auto const state = layout.release();
    EXPECT_FALSE(layout.created());

    DescriptorSetLayout const wrapped{context.core, gap_config(), state};
    EXPECT_EQ(wrapped.get(), vkLayout);
    EXPECT_EQ(wrapped.bindings().size(), 2);
}

CVK_TEST(DescriptorSetLayout, move_assignment_destroys_the_old_layout) {
    test::Context const context{};
    DescriptorSetLayout layout{context.core, {}, create};
    DescriptorSetLayout other{context.core, gap_config(), create};
    auto const vkLayout = other.get();

    layout = std::move(other);

    EXPECT_EQ(layout.get(), vkLayout);
    EXPECT_EQ(layout.bindings().size(), 2);
    EXPECT_FALSE(other.created());
}

CVK_TEST(DescriptorSetLayout, config_keeps_the_flags_in_binding_order) {
    static_assert([] {
        DescriptorSetLayout::Config config{};
        config.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
              .add(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 8,
                  VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT)
              .add(1, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        bool const added = config.bindingFlags.size() == 3 && config.bindingFlags[0] == 0
            && config.bindingFlags[1] == 0 && config.bindingFlags[2] == VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        config.remove(1);
        return added && config.bindingFlags.size() == 2
            && config.bindingFlags[1] == VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    }());
}

CVK_TEST(DescriptorSetLayout, config_keeps_the_flags_of_the_first_binding) {
    static_assert([] {
        DescriptorSetLayout::Config config{};
        config.add(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 8,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        return config.bindingFlags.size() == 1 && config.bindingFlags[0] == VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    }());
}

CVK_TEST(DescriptorSetLayout, config_without_flags_keeps_no_flags) {
    static_assert(gap_config().bindingFlags.empty());
}

CVK_TEST(DescriptorSetLayout, create_bindless) {
    test::Context const context{};
    constexpr VkDescriptorBindingFlags BINDLESS = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    DescriptorSetLayout::Config config{};
    config.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
          .add(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1024, BINDLESS);
    DescriptorSetLayout const layout{context.core, config, create};

    ASSERT_TRUE(layout.created());
    EXPECT_TRUE(layout.updateAfterBind());
    EXPECT_EQ(layout.bindingFlags(1), BINDLESS);
    EXPECT_EQ(layout.bindingFlags(0), 0);
    EXPECT_EQ(layout.variableBinding(), layout.binding(1));
}
}
