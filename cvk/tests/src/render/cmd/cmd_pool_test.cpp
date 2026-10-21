#include "context.hpp"
#include "test.hpp"

#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"

#include <type_traits>


namespace cvk {
namespace {
    CmdPool::Config pool_config(test::Context const& context) {
        return CmdPool::Config{.queueFamilyIndex = context.core.queue(0).familyIndex()};
    }
}

CVK_TEST(CmdPool, config_create_info) {
    constexpr CmdPool::Config config{.queueFamilyIndex = 2, .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT};
    constexpr auto info = config.createInfo();

    static_assert(info.queueFamilyIndex == 2);
    static_assert(info.flags == VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
}

CVK_TEST(CmdPool, config_is_an_aggregate_with_defaults) {
    static_assert(std::is_aggregate_v<CmdPool::Config>);

    constexpr CmdPool::Config empty{};
    static_assert(empty.queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
    static_assert(empty.flags == CmdPool::Config::DEFAULT_FLAGS);

    constexpr CmdPool::Config withFamily{.queueFamilyIndex = 3};
    static_assert(withFamily.queueFamilyIndex == 3);
    static_assert((withFamily.flags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) != 0);
}

CVK_TEST(CmdPool, base_constructor_does_not_create) {
    test::Context const context{};

    CmdPool const pool{context.core, pool_config(context)};

    EXPECT_FALSE(pool.created());
    EXPECT_TRUE(pool.unused());
}

CVK_TEST(CmdPool, create_and_destroy) {
    test::Context const context{};
    CmdPool pool{context.core, pool_config(context), create};

    ASSERT_TRUE(pool.created());
    EXPECT_EQ(&pool.core(), &context.core);

    pool.destroy();
    EXPECT_FALSE(pool.created());
}

CVK_TEST(CmdPool, counts_its_command_buffers) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};

    {
        PrimaryCmdBuffer primary{pool, create};
        SecondaryCmdBuffer const secondary{pool, create};
        EXPECT_FALSE(pool.unused());

        primary.destroy();
        EXPECT_FALSE(pool.unused());
    }

    EXPECT_TRUE(pool.unused());
}

CVK_TEST(CmdPool, release_of_a_buffer_stops_the_count) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
    PrimaryCmdBuffer primary{pool, create};

    auto const state = primary.release();
    EXPECT_TRUE(pool.unused());

    BaseCmdBuffer::destroy(context.core.deviceTable(), pool.get(), state.vkBuffer.get());
}

CVK_TEST(CmdPool, owner_of_a_released_buffer_counts_it) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
    PrimaryCmdBuffer primary{pool, create};

    auto const state = primary.release();
    pool.add();
    EXPECT_FALSE(pool.unused());

    BaseCmdBuffer::destroy(context.core.deviceTable(), pool.get(), state.vkBuffer.get());
    pool.remove();
    EXPECT_TRUE(pool.unused());
}

CVK_TEST(CmdPool, reset_returns_the_buffers_to_the_initial_state) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
    PrimaryCmdBuffer primary{pool, create};
    primary.begin();
    primary.end();

    pool.reset();
    EXPECT_EQ(pool.resets(), 1);

    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    primary.end();
}

CVK_TEST(CmdPool, reset_ends_the_recording) {
    test::Context const context{};
    CmdPool const pool{context.core, pool_config(context), create};
    PrimaryCmdBuffer primary{pool, create};
    SecondaryCmdBuffer secondary{pool, create};
    primary.begin();
    secondary.begin();

    pool.reset();

    EXPECT_FALSE(primary.recording());
    EXPECT_FALSE(secondary.recording());

    primary.begin();
    EXPECT_TRUE(primary.recording());
    primary.end();
}

CVK_TEST(CmdPool, release_and_wrap) {
    test::Context const context{};
    CmdPool pool{context.core, pool_config(context), create};
    auto const vkPool = pool.get();

    auto const state = pool.release();
    EXPECT_FALSE(pool.created());

    CmdPool const wrapped{context.core, pool_config(context), state};
    EXPECT_EQ(wrapped.get(), vkPool);
}

CVK_TEST(CmdPool, move_assignment_destroys_the_old_pool) {
    test::Context const context{};
    CmdPool pool{context.core, pool_config(context), create};
    CmdPool other{context.core, pool_config(context), create};
    auto const vkPool = other.get();

    pool = std::move(other);

    EXPECT_EQ(pool.get(), vkPool);
    EXPECT_FALSE(other.created());
}
}
