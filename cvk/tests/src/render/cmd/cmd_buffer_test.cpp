#include "context.hpp"
#include "test.hpp"

#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/fence.hpp"

#include <array>
#include <type_traits>


namespace cvk {
namespace {
    CmdPool::Config pool_config(test::Context const& context) {
        return CmdPool::Config::Default(context.core.queue(0));
    }

    void submit_and_wait(test::Context const& context, PrimaryCmdBuffer const& primary) {
        Fence const fence{context.core, 0};

        VkCommandBufferSubmitInfo const bufferInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr,
            .commandBuffer = primary.get(),
            .deviceMask = 0,
        };
        VkSubmitInfo2 const submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = 0,
            .pWaitSemaphoreInfos = nullptr,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &bufferInfo,
            .signalSemaphoreInfoCount = 0,
            .pSignalSemaphoreInfos = nullptr,
        };

        context.core.queue(0).raw_submit(submitInfo, fence.get());
        fence.wait();
    }
}

CVK_TEST(CmdBuffer, inheritance_rendering_info) {
    static_assert([] {
        SecondaryCmdBuffer::Inheritance inheritance{
            .colorFormats = {VK_FORMAT_B8G8R8A8_UNORM},
            .depthFormat = VK_FORMAT_D16_UNORM,
            .samples = VK_SAMPLE_COUNT_4_BIT,
        };
        auto const info = inheritance.renderingInfo();
        return info.colorAttachmentCount == 1 && info.pColorAttachmentFormats == inheritance.colorFormats.data()
            && info.depthAttachmentFormat == VK_FORMAT_D16_UNORM && info.rasterizationSamples == VK_SAMPLE_COUNT_4_BIT;
    }());
}

CVK_TEST(CmdBuffer, delete_only_through_the_derived_class) {
    static_assert(!std::is_destructible_v<BaseCmdBuffer>);
    static_assert(std::is_destructible_v<PrimaryCmdBuffer>);
    static_assert(std::is_destructible_v<SecondaryCmdBuffer>);
}

CVK_TEST(CmdBuffer, base_constructor_does_not_create) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};

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
    CmdPool const pool{context.core, pool_config(context), create};
    PrimaryCmdBuffer primary{pool, create};

    ASSERT_TRUE(primary.created());
    EXPECT_EQ(&primary.core(), &context.core);
    EXPECT_FALSE(primary.recording());

    primary.destroy();
    EXPECT_FALSE(primary.created());
}

CVK_TEST(CmdBuffer, begin_and_end) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
    PrimaryCmdBuffer primary{pool, create};

    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    EXPECT_TRUE(primary.recording());

    primary.end();
    EXPECT_FALSE(primary.recording());
}

CVK_TEST(CmdBuffer, submit) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
    PrimaryCmdBuffer primary{pool, create};
    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    primary.end();

    submit_and_wait(context, primary);
}

CVK_TEST(CmdBuffer, reset_and_record_again) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
    PrimaryCmdBuffer primary{pool, create};
    primary.begin();

    primary.reset();
    EXPECT_FALSE(primary.recording());

    primary.begin();
    primary.end();
    submit_and_wait(context, primary);
}

CVK_TEST(CmdBuffer, execute_secondaries) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
    std::array<SecondaryCmdBuffer, 2> secondaries{
        SecondaryCmdBuffer{pool, create},
        SecondaryCmdBuffer{pool, create},
    };
    for(auto& secondary : secondaries) {
        secondary.begin();
        secondary.end();
    }

    PrimaryCmdBuffer primary{pool, create};
    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    primary.execute(secondaries);
    primary.end();

    submit_and_wait(context, primary);
}

CVK_TEST(CmdBuffer, secondary_begin_with_rendering_inheritance) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
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

CVK_TEST(CmdBuffer, release_and_wrap) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
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
    CmdPool const pool{context.core, pool_config(context), create};
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
    CmdPool const pool{context.core, pool_config(context), create};
    PrimaryCmdBuffer primary{pool, create};
    PrimaryCmdBuffer other{pool, create};
    auto const vkBuffer = other.get();

    primary = std::move(other);

    EXPECT_EQ(primary.get(), vkBuffer);
    EXPECT_FALSE(other.created());

    primary.destroy();
    EXPECT_TRUE(pool.unused());
}
}
