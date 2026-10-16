#include "context.hpp"
#include "test.hpp"

#include "cvk/base/physical_device.hpp"
#include "cvk/data/img/sampler.hpp"


namespace cvk {
CVK_TEST(Sampler, config_create_info) {
    Sampler::Config config{};

    auto info = config.createInfo();
    EXPECT_EQ(info.anisotropyEnable, VK_TRUE);
    EXPECT_EQ(info.compareEnable, VK_FALSE);

    config.maxAnisotropy = 0.f;
    config.compareOp = VK_COMPARE_OP_NEVER;
    info = config.createInfo();
    EXPECT_EQ(info.anisotropyEnable, VK_FALSE);
    EXPECT_EQ(info.compareEnable, VK_TRUE);
    EXPECT_EQ(info.compareOp, VK_COMPARE_OP_NEVER);
}

CVK_TEST(Sampler, config_create_info_is_constexpr) {
    static_assert(Sampler::Config{}.createInfo().anisotropyEnable == VK_TRUE);
    static_assert(Sampler::Config{.compareOp = VK_COMPARE_OP_LESS}.createInfo().compareOp == VK_COMPARE_OP_LESS);
}

CVK_TEST(Sampler, base_constructor_clamps_anisotropy) {
    test::Context const context{};
    auto const limit = context.core.physicalDevice().limits().maxSamplerAnisotropy;

    Sampler const sampler{context.core, Sampler::Config{.maxAnisotropy = limit * 2}};

    EXPECT_FALSE(sampler.created());
    EXPECT_EQ(sampler.config().maxAnisotropy, limit);
}

CVK_TEST(Sampler, create_default) {
    test::Context const context{};

    Sampler const sampler{context.core, {}, create};

    EXPECT_TRUE(sampler.created());
}

CVK_TEST(Sampler, create_compare_without_anisotropy) {
    test::Context const context{};

    Sampler const sampler{
        context.core,
        Sampler::Config{.maxAnisotropy = 0.f, .compareOp = VK_COMPARE_OP_LESS_OR_EQUAL},
        create
    };

    EXPECT_TRUE(sampler.created());
}

CVK_TEST(Sampler, release_and_wrap) {
    test::Context const context{};
    Sampler sampler{context.core, {}, create};
    auto const vkSampler = sampler.get();

    auto const state = sampler.release();
    EXPECT_FALSE(sampler.created());

    Sampler const wrapped{context.core, {}, state};
    EXPECT_EQ(wrapped.get(), vkSampler);
}

CVK_TEST(Sampler, move_assignment_destroys_the_old_sampler) {
    test::Context const context{};
    Sampler sampler{context.core, {}, create};
    Sampler other{context.core, {}, create};
    auto const vkSampler = other.get();

    sampler = std::move(other);

    EXPECT_EQ(sampler.get(), vkSampler);
    EXPECT_FALSE(other.created());
}
}
