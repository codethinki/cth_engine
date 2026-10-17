#include "context.hpp"
#include "test.hpp"

#include "cvk/base/physical_device.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"

#include <algorithm>
#include <array>


namespace cvk {
namespace {
    constexpr size_t SIZE = 256;

    /**
     * @brief staging buffer, the cpu writes it once
     */
    constexpr BaseBuffer::Config STAGING_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
    };

    /**
     * @brief records into a primary command buffer, submits and waits
     */
    template<class Record>
    void record_and_submit(test::Context const& context, Record record) {
        CmdPool const pool{context.core, CmdPool::Config::Default(context.core.queue(0)), create};
        PrimaryCmdBuffer primary{pool, create};

        primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        record(primary);
        test::host_read_barrier(context, primary);
        primary.end();

        test::submit_and_wait(context, primary);
    }

    /**
     * @brief buffer the cpu reads back
     */
    constexpr BaseBuffer::Config READBACK_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT},
    };

    /**
     * @brief gpu only buffer
     */
    constexpr BaseBuffer::Config DEVICE_CONFIG{.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
}

CVK_TEST(BaseBuffer, config_create_info) {
    constexpr auto info = STAGING_CONFIG.createInfo(SIZE);

    static_assert(info.size == SIZE);
    static_assert(info.usage == VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    static_assert(info.flags == 0);
    static_assert(info.sharingMode == VK_SHARING_MODE_EXCLUSIVE);
    static_assert(info.queueFamilyIndexCount == 0 && info.pQueueFamilyIndices == nullptr);
    static_assert(STAGING_CONFIG.memory.autoUsage());
}

CVK_TEST(BaseBuffer, config_one_queue_family_is_exclusive) {
    static_assert([] {
        BaseBuffer::Config config{.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .queueFamilies = {2}};
        auto const info = config.createInfo(SIZE);
        return info.sharingMode == VK_SHARING_MODE_EXCLUSIVE && info.queueFamilyIndexCount == 1;
    }());
}

CVK_TEST(BaseBuffer, config_queue_families_are_concurrent) {
    static_assert([] {
        BaseBuffer::Config config{
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .flags = VK_BUFFER_CREATE_PROTECTED_BIT,
            .queueFamilies = {0, 2},
        };
        auto const info = config.createInfo(SIZE);
        return info.sharingMode == VK_SHARING_MODE_CONCURRENT && info.queueFamilyIndexCount == 2
            && info.pQueueFamilyIndices == config.queueFamilies.data() && info.flags == VK_BUFFER_CREATE_PROTECTED_BIT;
    }());
}

CVK_TEST(BaseBuffer, config_debug_check_is_constexpr) {
    static_assert([] {
        BaseBuffer::Config::debug_check({.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .queueFamilies = {1, 0}});
        return true;
    }());
}

CVK_TEST(BaseBuffer, base_constructor_does_not_create) {
    test::Context const context{};

    BaseBuffer const buffer{context.core, STAGING_CONFIG};

    EXPECT_FALSE(buffer.created());
    EXPECT_FALSE(buffer.memory().created());
}

CVK_TEST(BaseBuffer, create_and_destroy) {
    test::Context const context{};
    BaseBuffer buffer{context.core, DEVICE_CONFIG, SIZE};

    ASSERT_TRUE(buffer.created());
    EXPECT_EQ(&buffer.core(), &context.core);
    EXPECT_EQ(buffer.size(), SIZE);
    ASSERT_TRUE(buffer.memory().created());
    EXPECT_GE(buffer.memory().size(), SIZE);

    buffer.destroy();
    EXPECT_FALSE(buffer.created());
    EXPECT_FALSE(buffer.memory().created());
    EXPECT_EQ(buffer.size(), 0);
}

CVK_TEST(BaseBuffer, create_concurrent) {
    test::Context const context{};
    auto const familyCount = context.core.physicalDevice().queueFamilies().size();
    if(familyCount < 2) GTEST_SKIP() << "needs 2 queue families";

    auto config = DEVICE_CONFIG;
    config.queueFamilies = {0, 1};

    BaseBuffer const buffer{context.core, config, SIZE};

    EXPECT_TRUE(buffer.created());
}

CVK_TEST(BaseBuffer, auto_usage_picks_host_visible_memory_for_host_access) {
    test::Context const context{};

    BaseBuffer const buffer{context.core, STAGING_CONFIG, SIZE};

    EXPECT_NE(buffer.memory().properties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0u);
}

CVK_TEST(BaseBuffer, write_and_read_through_the_memory) {
    test::Context const context{};
    BaseBuffer const buffer{context.core, READBACK_CONFIG, SIZE};
    std::array<char, SIZE> data{};
    std::ranges::fill(data, 'x');

    buffer.memory().write(data);

    auto const mapped = buffer.memory().map(SIZE);
    buffer.memory().invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, data));
    buffer.memory().unmap();
}

CVK_TEST(BaseBuffer, copy_a_range) {
    test::Context const context{};
    BaseBuffer const staging{context.core, STAGING_CONFIG, SIZE};
    BaseBuffer const readback{context.core, READBACK_CONFIG, SIZE};
    std::array<char, 64> data{};
    std::ranges::fill(data, 'c');
    staging.memory().write(data, 16);

    record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        readback.copy(primary, staging, data.size(), 16, 32);
    });

    auto const mapped = readback.memory().map(data.size(), 32);
    readback.memory().invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, data));
    readback.memory().unmap();
}

CVK_TEST(BaseBuffer, copy_whole_size_fits_both_buffers) {
    test::Context const context{};
    BaseBuffer const staging{context.core, STAGING_CONFIG, SIZE};
    BaseBuffer const readback{context.core, READBACK_CONFIG, SIZE / 2};
    std::array<char, SIZE> data{};
    std::ranges::fill(data, 'w');
    staging.memory().write(data);

    record_and_submit(context, [&](PrimaryCmdBuffer const& primary) { readback.copy(primary, staging); });

    auto const mapped = readback.memory().map(SIZE / 2);
    readback.memory().invalidate();
    EXPECT_TRUE(std::ranges::all_of(mapped, [](char const c) { return c == 'w'; }));
    readback.memory().unmap();
}

CVK_TEST(BaseBuffer, stage_the_whole_staging_buffer) {
    test::Context const context{};
    BaseBuffer const staging{context.core, STAGING_CONFIG, 64};
    BaseBuffer const readback{context.core, READBACK_CONFIG, SIZE};
    std::array<char, 64> data{};
    std::ranges::fill(data, 's');
    staging.memory().write(data);

    record_and_submit(context, [&](PrimaryCmdBuffer const& primary) { readback.stage(primary, staging, 8); });

    auto const mapped = readback.memory().map(data.size(), 8);
    readback.memory().invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, data));
    readback.memory().unmap();
}

CVK_TEST(BaseBuffer, descriptor_info) {
    test::Context const context{};
    BaseBuffer const buffer{context.core, DEVICE_CONFIG, SIZE};

    auto const whole = buffer.descriptorInfo();
    EXPECT_EQ(whole.buffer, buffer.get());
    EXPECT_EQ(whole.offset, 0);
    EXPECT_EQ(whole.range, SIZE);

    auto const end = buffer.descriptorInfo(VK_WHOLE_SIZE, 64);
    EXPECT_EQ(end.offset, 64);
    EXPECT_EQ(end.range, SIZE - 64);

    auto const part = buffer.descriptorInfo(16, 32);
    EXPECT_EQ(part.range, 16);
}

CVK_TEST(BaseBuffer, release_and_wrap) {
    test::Context const context{};
    BaseBuffer buffer{context.core, DEVICE_CONFIG, SIZE};
    auto const vkBuffer = buffer.get();
    auto const vmaAllocation = buffer.memory().get();

    auto const state = buffer.release();
    EXPECT_FALSE(buffer.created());
    EXPECT_FALSE(buffer.memory().created());
    ASSERT_TRUE(state.memory.has_value());

    BaseBuffer const wrapped{context.core, DEVICE_CONFIG, state};
    EXPECT_EQ(wrapped.get(), vkBuffer);
    EXPECT_EQ(wrapped.memory().get(), vmaAllocation);
    EXPECT_EQ(wrapped.size(), SIZE);
}

CVK_TEST(BaseBuffer, wrap_without_memory_destroys_only_the_buffer) {
    test::Context const context{};
    BaseBuffer buffer{context.core, DEVICE_CONFIG, SIZE};
    auto state = buffer.release();

    // another owner frees the memory, after the buffer
    Memory const memory{context.core, DEVICE_CONFIG.memory, *state.memory};
    state.memory = std::nullopt;

    {
        BaseBuffer const wrapped{context.core, DEVICE_CONFIG, state};
        EXPECT_FALSE(wrapped.memory().created());
        EXPECT_EQ(wrapped.size(), SIZE);
    }

    EXPECT_TRUE(memory.created());
}

CVK_TEST(BaseBuffer, move_assignment_destroys_the_old_buffer) {
    test::Context const context{};
    BaseBuffer buffer{context.core, DEVICE_CONFIG, SIZE};
    BaseBuffer other{context.core, DEVICE_CONFIG, SIZE};
    auto const vkBuffer = other.get();

    buffer = std::move(other);

    EXPECT_EQ(buffer.get(), vkBuffer);
    EXPECT_TRUE(buffer.memory().created());
    EXPECT_FALSE(other.created());
    EXPECT_FALSE(other.memory().created());
}
}
