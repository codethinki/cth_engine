#include "context.hpp"
#include "test.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/descriptor/descriptor_pool.hpp"
#include "cvk/data/descriptor/descriptor_set.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/data/img/sampler.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"

#include <array>


namespace cvk {
namespace {
    constexpr VkDescriptorBindingFlags BINDLESS = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    /**
     * @brief binding 0: uniform buffer, binding 1: combined image sampler
     */
    DescriptorSetLayout::Config buffer_and_texture() {
        DescriptorSetLayout::Config config{};
        config.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
              .add(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        return config;
    }

    /**
     * @brief binding 0: up to 64 combined image samplers, bindless flags
     */
    DescriptorSetLayout::Config bindless() {
        DescriptorSetLayout::Config config{};
        config.add(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 64, BINDLESS);
        return config;
    }

    constexpr Image::Config TEXTURE_CONFIG{.format = VK_FORMAT_R8G8B8A8_UNORM, .usage = VK_IMAGE_USAGE_SAMPLED_BIT};
}

CVK_TEST(DescriptorSet, base_constructor_does_not_create) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffer_and_texture(), create};
    DescriptorPool const pool{context.core, DescriptorPool::Config{{&layout}}, create};

    DescriptorSet const set{pool, layout};

    EXPECT_FALSE(set.created());
    EXPECT_EQ(&set.pool(), &pool);
    EXPECT_EQ(&set.layout(), &layout);
    EXPECT_TRUE(pool.unused());
}

CVK_TEST(DescriptorSet, destroy_frees_with_a_freeing_pool) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffer_and_texture(), create};
    DescriptorPool const pool{
        context.core,
        DescriptorPool::Config{{{&layout}}, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT},
        create
    };

    DescriptorSet set{pool, layout, create};
    ASSERT_TRUE(set.created());
    EXPECT_FALSE(pool.unused());

    set.destroy();
    EXPECT_FALSE(set.created());
    EXPECT_TRUE(pool.unused());

    // the freed set is available again
    set.create();
    EXPECT_TRUE(set.created());
}

CVK_TEST(DescriptorSet, destroy_without_free_only_forgets) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffer_and_texture(), create};
    DescriptorPool const pool{context.core, DescriptorPool::Config{{&layout}}, create};

    DescriptorSet set{pool, layout, create};
    set.destroy();

    EXPECT_FALSE(set.created());
    EXPECT_TRUE(pool.unused());
    pool.reset();
}

CVK_TEST(DescriptorSet, pool_reset_frees_the_set) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffer_and_texture(), create};
    DescriptorPool const pool{context.core, DescriptorPool::Config{{&layout}}, create};
    DescriptorSet set{pool, layout, create};

    pool.reset();

    EXPECT_FALSE(set.created());
    EXPECT_TRUE(pool.unused());

    set.create();
    EXPECT_TRUE(set.created());
}

CVK_TEST(DescriptorSet, write_buffer_and_image) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffer_and_texture(), create};
    DescriptorPool const pool{context.core, DescriptorPool::Config{{&layout}}, create};
    DescriptorSet const set{pool, layout, create};

    BaseBuffer const buffer{context.core, {.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT}, 64};
    Image const image{context.core, TEXTURE_CONFIG, VkExtent2D{4, 4}};
    ImageView const view{image, {}, create};
    Sampler const sampler{context.core, {}, create};

    set.write(
        std::array{
            DescriptorWrite{.binding = 0, .buffers = {{&buffer, 0, 16}}},
            DescriptorWrite{.binding = 1, .images = {{.view = &view, .sampler = &sampler}}},
        }
    );
}

CVK_TEST(DescriptorSet, variable_count) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, bindless(), create};
    DescriptorPool const pool{context.core, DescriptorPool::Config{{&layout, 1, 8}}, create};
    DescriptorSet const set{pool, layout, 8, create};

    Image const image{context.core, TEXTURE_CONFIG, VkExtent2D{4, 4}};
    ImageView const view{image, {}, create};
    Sampler const sampler{context.core, {}, create};

    EXPECT_EQ(set.variableCount(), 8);
    set.write(
        std::array{
            DescriptorWrite{
                .binding = 0,
                .arrayElement = 6,
                .images = {{.view = &view, .sampler = &sampler}, {.view = &view, .sampler = &sampler}},
            },
        }
    );
}

CVK_TEST(DescriptorSet, release_and_wrap) {
    test::Context const context{};
    DescriptorSetLayout const layout{context.core, buffer_and_texture(), create};
    DescriptorPool const pool{
        context.core,
        DescriptorPool::Config{{{&layout}}, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT},
        create
    };
    DescriptorSet set{pool, layout, create};
    auto const vkSet = set.get();

    auto const state = set.release();
    EXPECT_FALSE(set.created());
    EXPECT_TRUE(pool.unused());

    DescriptorSet const wrapped{pool, layout, 0, state};
    EXPECT_EQ(wrapped.get(), vkSet);
    EXPECT_FALSE(pool.unused());
}
}
