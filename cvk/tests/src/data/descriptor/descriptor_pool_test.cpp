#include "context.hpp"
#include "test.hpp"

#include "cvk/data/descriptor/descriptor_pool.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"

#include <algorithm>


namespace cvk {
namespace {
    /**
     * @brief binding 0: 2 uniform buffers, binding 1: 1 combined image sampler
     */
    DescriptorSetLayout::Config buffers_and_texture() {
        DescriptorSetLayout::Config config{};
        config.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 2)
              .add(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        return config;
    }

    /**
     * @brief binding 0: up to 1024 combined image samplers, bindless flags
     */
    DescriptorSetLayout::Config bindless() {
        DescriptorSetLayout::Config config{};
        config.add(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1024, test::BINDLESS);
        return config;
    }

    uint32_t count_of(DescriptorPool::Config const& config, VkDescriptorType type) {
        auto const it = std::ranges::find(config.sizes, type, &VkDescriptorPoolSize::type);
        return it != config.sizes.end() ? it->descriptorCount : 0;
    }
}

CVK_TEST(DescriptorPool, config_add_counts_the_descriptors_of_the_layout) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffers_and_texture(), create};

    DescriptorPool::Config config{};
    config.add(layout, 3);

    EXPECT_EQ(config.maxSets, 3);
    EXPECT_EQ(config.sizes.size(), 2);
    EXPECT_EQ(count_of(config, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), 6);
    EXPECT_EQ(count_of(config, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER), 3);
    EXPECT_EQ(config.flags, 0);
}

CVK_TEST(DescriptorPool, config_add_takes_the_variable_count_and_update_after_bind) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, bindless(), create};

    DescriptorPool::Config config{};
    config.add(layout, 2, 100);

    EXPECT_EQ(config.maxSets, 2);
    EXPECT_EQ(count_of(config, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER), 200);
    EXPECT_NE(config.flags & VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, 0);
}

CVK_TEST(DescriptorPool, config_is_an_aggregate) {
    static_assert(std::is_aggregate_v<DescriptorPool::Config>);

    DescriptorPool::Config const config{.maxSets = 2, .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT};

    EXPECT_EQ(config.maxSets, 2);
    EXPECT_TRUE(config.sizes.empty());
    EXPECT_EQ(config.flags, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
}

CVK_TEST(DescriptorPool, for_layouts_counts_every_layout) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffers_and_texture(), create};
    DescriptorSetLayout const textures{context.core, bindless(), create};

    auto const config = DescriptorPool::Config::ForLayouts(
        {{&layout, 2}, {&textures, 1, 100}},
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
    );

    EXPECT_EQ(config.maxSets, 3);
    EXPECT_EQ(count_of(config, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), 4);
    EXPECT_EQ(count_of(config, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER), 102);
    EXPECT_NE(config.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 0);
    EXPECT_NE(config.flags & VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, 0);
}

CVK_TEST(DescriptorPool, create_and_destroy) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffers_and_texture(), create};

    DescriptorPool pool{context.core, DescriptorPool::Config::ForLayouts({{&layout, 4}}), create};

    ASSERT_TRUE(pool.created());
    EXPECT_TRUE(pool.unused());
    EXPECT_FALSE(pool.freesSets());

    pool.destroy();
    EXPECT_FALSE(pool.created());
}

CVK_TEST(DescriptorPool, reset_counts) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffers_and_texture(), create};
    DescriptorPool const pool{context.core, DescriptorPool::Config::ForLayouts({{&layout}}), create};

    pool.reset();
    pool.reset();

    EXPECT_EQ(pool.resets(), 2);
}

CVK_TEST(DescriptorPool, release_and_wrap) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffers_and_texture(), create};
    auto const config = DescriptorPool::Config::ForLayouts({{&layout}});
    DescriptorPool pool{context.core, config, create};
    auto const vkPool = pool.get();

    auto const state = pool.release();
    EXPECT_FALSE(pool.created());

    DescriptorPool const wrapped{context.core, config, state};
    EXPECT_EQ(wrapped.get(), vkPool);
}
}
