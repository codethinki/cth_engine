#include "context.hpp"
#include "test.hpp"

#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/barrier.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>


namespace cvk {
using test::pool_config;

CVK_TEST(CmdBuffer, inheritance_rendering_info) {
    static_assert(
        [] {
            SecondaryCmdBuffer::Inheritance inheritance{
                .colorFormats = {VK_FORMAT_B8G8R8A8_UNORM},
                .depthFormat = VK_FORMAT_D16_UNORM,
                .samples = VK_SAMPLE_COUNT_4_BIT,
            };
            auto const info = inheritance.renderingInfo();
            return info.colorAttachmentCount == 1
                && info.pColorAttachmentFormats == inheritance.colorFormats.data()
                && info.depthAttachmentFormat == VK_FORMAT_D16_UNORM
                && info.rasterizationSamples == VK_SAMPLE_COUNT_4_BIT;
        }()
    );
}

CVK_TEST(CmdBuffer, push_constant_data) {
    static_assert(push_constant_range<std::array<float, 4>>);
    static_assert(push_constant_range<std::span<float const>>);
    static_assert(push_constant_range<std::vector<uint32_t>>);
    static_assert(!push_constant_range<std::vector<std::string>>);

    static_assert(push_constant_data<uint32_t>);
    static_assert(push_constant_data<std::span<float const>>);
    static_assert(!push_constant_data<float const*>);
    static_assert(!push_constant_data<std::vector<std::string>>);
}

CVK_TEST(CmdBuffer, delete_only_through_the_derived_class) {
    static_assert(!std::is_destructible_v<BaseCmdBuffer>);
    static_assert(std::is_destructible_v<PrimaryCmdBuffer>);
    static_assert(std::is_destructible_v<SecondaryCmdBuffer>);
}

CVK_TEST(CmdBuffer, base_constructor_does_not_create) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};

    PrimaryCmdBuffer const primary{pool};
    SecondaryCmdBuffer const secondary{pool};

    EXPECT_FALSE(primary.created());
    EXPECT_FALSE(secondary.created());
    EXPECT_EQ(primary.level(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    EXPECT_EQ(secondary.level(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    EXPECT_EQ(&primary.pool(), &pool);
}

CVK_TEST(CmdBuffer, create_and_destroy) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};

    ASSERT_TRUE(primary.created());
    EXPECT_EQ(&primary.core(), &context.core);
    EXPECT_FALSE(primary.recording());

    primary.destroy();
    EXPECT_FALSE(primary.created());
}

CVK_TEST(CmdBuffer, begin_and_end) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};

    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    EXPECT_TRUE(primary.recording());

    primary.end();
    EXPECT_FALSE(primary.recording());
}

CVK_TEST(CmdBuffer, opt_end_ends_only_a_recording) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};

    primary.optEnd();
    EXPECT_FALSE(primary.recording());

    primary.begin();
    primary.optEnd();
    EXPECT_FALSE(primary.recording());

    test::submit_and_wait(context, primary);
}

CVK_TEST(CmdBuffer, submit) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};
    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    primary.end();

    test::submit_and_wait(context, primary);
}

CVK_TEST(CmdBuffer, reset_and_record_again) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};
    primary.begin();

    primary.reset();
    EXPECT_FALSE(primary.recording());

    primary.begin();
    primary.end();
    test::submit_and_wait(context, primary);
}

CVK_TEST(CmdBuffer, execute_secondaries) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    std::array<SecondaryCmdBuffer, 2> secondaries{
        SecondaryCmdBuffer{pool, create},
        SecondaryCmdBuffer{pool, create},
    };
    for(auto& secondary : secondaries) {
        secondary.begin();
        secondary.end();
    }

    std::vector<cth::ref_wrapper<SecondaryCmdBuffer const>> const refs{
        secondaries.begin(),
        secondaries.end()
    };

    PrimaryCmdBuffer primary{pool, create};
    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    primary.execute(refs);
    primary.end();

    test::submit_and_wait(context, primary);
}

CVK_TEST(CmdBuffer, execute_secondaries_of_a_braced_list) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    // secondaries[1] is not recorded, the iterator pair [&secondaries[0], &secondaries[2]) would execute it
    std::array<SecondaryCmdBuffer, 3> secondaries{
        SecondaryCmdBuffer{pool, create},
        SecondaryCmdBuffer{pool, create},
        SecondaryCmdBuffer{pool, create},
    };
    for(auto* secondary : {&secondaries[0], &secondaries[2]}) {
        secondary->begin();
        secondary->end();
    }

    PrimaryCmdBuffer primary{pool, create};
    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    primary.execute({secondaries[0], secondaries[2]});
    primary.end();

    test::submit_and_wait(context, primary);
}

CVK_TEST(CmdBuffer, secondary_begin_with_rendering_inheritance) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    SecondaryCmdBuffer secondary{pool, create};

    secondary.begin(
        SecondaryCmdBuffer::Inheritance{
            .colorFormats = {VK_FORMAT_R8G8B8A8_UNORM},
            .depthFormat = VK_FORMAT_D16_UNORM,
        }
    );
    EXPECT_TRUE(secondary.recording());

    secondary.end();
}

CVK_TEST(CmdBuffer, secondary_continues_the_inherited_rendering) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    SecondaryCmdBuffer secondary{pool, create};

    secondary.begin();
    EXPECT_FALSE(secondary.insideRendering());
    EXPECT_EQ(secondary.inheritedRendering(), nullptr);
    secondary.end();

    secondary.begin(SecondaryCmdBuffer::Inheritance{.colorFormats = {VK_FORMAT_R8G8B8A8_UNORM}});
    EXPECT_TRUE(secondary.insideRendering());
    EXPECT_EQ(secondary.rendering(), nullptr);
    ASSERT_NE(secondary.inheritedRendering(), nullptr);
    EXPECT_EQ(secondary.inheritedRendering()->colorFormats, std::vector{VK_FORMAT_R8G8B8A8_UNORM});
    EXPECT_EQ(secondary.renderingViewMask(), 0);
    secondary.end();

    // the next begin forgets the inheritance
    secondary.begin();
    EXPECT_FALSE(secondary.insideRendering());
    secondary.end();
}

CVK_TEST(CmdBuffer, secondary_reports_the_inherited_view_mask) {
    auto config = test::Context::CONFIG;
    config.features.multiview = true;
    std::optional<test::Context> context{};
    try { context.emplace(config); }
    catch(cvk_exception const&) {
        GTEST_SKIP() << "needs multiview";
    }

    CmdPool const pool{context->core, pool_config(context->core), create};
    SecondaryCmdBuffer secondary{pool, create};

    secondary.begin(
        SecondaryCmdBuffer::Inheritance{
            .colorFormats = {VK_FORMAT_R8G8B8A8_UNORM},
            .viewMask = 0b11
        }
    );
    EXPECT_EQ(secondary.renderingViewMask(), 0b11);
    secondary.end();
}

CVK_TEST(CmdBuffer, primary_is_inside_its_rendering) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};
    Image const image{
        context.core,
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        },
        VkExtent2D{4, 4}
    };
    ImageView const view{image, {}, create};

    primary.begin();
    EXPECT_FALSE(primary.insideRendering());

    Barrier transition{};
    transition.addTransition(
        image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_UNDEFINED
    );
    transition.execute(primary);
    primary.beginRendering(
        {
            .colorAttachments = {
                RenderingAttachment{
                    .view = view,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                },
            },
        }
    );
    EXPECT_TRUE(primary.insideRendering());
    EXPECT_EQ(primary.renderingViewMask(), 0);

    primary.endRendering();
    EXPECT_FALSE(primary.insideRendering());
    primary.end();
}

CVK_TEST(CmdBuffer, release_and_wrap) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};
    auto const vkBuffer = primary.get();

    auto const state = primary.release();
    EXPECT_FALSE(primary.created());

    PrimaryCmdBuffer const wrapped{pool, state};
    EXPECT_EQ(wrapped.get(), vkBuffer);
    EXPECT_FALSE(pool.unused());
}

CVK_TEST(CmdBuffer, moved_from_buffer_does_not_record) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};
    primary.begin();

    PrimaryCmdBuffer moved{std::move(primary)};

    EXPECT_FALSE(primary.recording());
    EXPECT_TRUE(moved.recording());

    primary.create();
    EXPECT_FALSE(primary.recording());

    moved.end();
}

CVK_TEST(CmdBuffer, move_assignment_frees_the_old_buffer) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};
    PrimaryCmdBuffer other{pool, create};
    auto const vkBuffer = other.get();

    primary = std::move(other);

    EXPECT_EQ(primary.get(), vkBuffer);
    EXPECT_FALSE(other.created());

    primary.destroy();
    EXPECT_TRUE(pool.unused());
}

CVK_TEST(CmdBuffer, release_fills_the_pool) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};

    auto const state = primary.release();
    EXPECT_EQ(state.vkPool.get(), pool.get());

    cvk::destroy(context.core.deviceTable(), state.vkPool.get(), state.vkBuffer.get());
}

CVK_TEST(CmdBuffer, decompose_returns_the_pool_and_the_released_handle) {
    // a leak shows up when the shared device is destroyed
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context.core), create};
    PrimaryCmdBuffer primary{pool, create};
    auto const vkBuffer = primary.get();

    auto const [buffer] = primary.release().decompose();
    auto const [vkPool, vkReleased] = buffer;
    EXPECT_EQ(vkPool, pool.get());
    EXPECT_EQ(vkReleased, vkBuffer);

    cvk::destroy(context.core.deviceTable(), vkPool, vkReleased);
}
}
