#include "context.hpp"
#include "test.hpp"

#include "cvk/base/physical_device.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>


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
    EXPECT_EQ(buffer.memory(), nullptr);
}

CVK_TEST(BaseBuffer, create_and_destroy) {
    test::Context const context{};
    BaseBuffer buffer{context.core, DEVICE_CONFIG, SIZE};

    ASSERT_TRUE(buffer.created());
    EXPECT_EQ(&buffer.core(), &context.core);
    EXPECT_EQ(buffer.size(), SIZE);
    ASSERT_NE(buffer.memory(), nullptr);
    EXPECT_GE(buffer.memory()->size(), SIZE);

    buffer.destroy();
    EXPECT_FALSE(buffer.created());
    EXPECT_EQ(buffer.memory(), nullptr);
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

    EXPECT_NE(buffer.memory()->properties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0u);
}

CVK_TEST(BaseBuffer, write_and_read) {
    test::Context const context{};
    BaseBuffer const buffer{context.core, READBACK_CONFIG, SIZE};
    std::array<std::byte, SIZE> data{};
    std::ranges::fill(data, std::byte{'x'});

    buffer.write(data);

    auto const mapped = buffer.map();
    buffer.invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, data));
}

CVK_TEST(BaseBuffer, copy_a_range) {
    test::Context const context{};
    BaseBuffer const staging{context.core, STAGING_CONFIG, SIZE};
    BaseBuffer const readback{context.core, READBACK_CONFIG, SIZE};
    std::array<std::byte, 64> data{};
    std::ranges::fill(data, std::byte{'c'});
    staging.write(data, 16);

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        readback.copy(primary, staging, {.srcOffset = 16, .dstOffset = 32, .size = data.size()});
        test::host_read_barrier(context, primary);
    });

    auto const mapped = readback.map({.offset = 32, .size = data.size()});
    readback.invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, data));
}

CVK_TEST(BaseBuffer, copy_whole_size_fits_both_buffers) {
    test::Context const context{};
    BaseBuffer const staging{context.core, STAGING_CONFIG, SIZE};
    BaseBuffer const readback{context.core, READBACK_CONFIG, SIZE / 2};
    std::array<std::byte, SIZE> data{};
    std::ranges::fill(data, std::byte{'w'});
    staging.write(data);

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        readback.copy(primary, staging);
        test::host_read_barrier(context, primary);
    });

    auto const mapped = readback.map();
    readback.invalidate();
    EXPECT_TRUE(std::ranges::all_of(mapped, [](std::byte b) { return b == std::byte{'w'}; }));
}

CVK_TEST(BaseBuffer, copy_the_whole_source_to_an_offset) {
    test::Context const context{};
    BaseBuffer const staging{context.core, STAGING_CONFIG, 64};
    BaseBuffer const readback{context.core, READBACK_CONFIG, SIZE};
    std::array<std::byte, 64> data{};
    std::ranges::fill(data, std::byte{'s'});
    staging.write(data);

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        readback.copy(primary, staging, {.dstOffset = 8});
        test::host_read_barrier(context, primary);
    });

    auto const mapped = readback.map({.offset = 8, .size = data.size()});
    readback.invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, data));
}

CVK_TEST(BaseBuffer, release_and_wrap) {
    test::Context const context{};
    BaseBuffer buffer{context.core, DEVICE_CONFIG, SIZE};
    auto const vkBuffer = buffer.get();
    auto const vmaAllocation = buffer.memory()->get();

    auto const state = buffer.release();
    EXPECT_FALSE(buffer.created());
    EXPECT_EQ(buffer.memory(), nullptr);
    ASSERT_TRUE(state.memory.has_value());

    BaseBuffer const wrapped{context.core, DEVICE_CONFIG, state};
    EXPECT_EQ(wrapped.get(), vkBuffer);
    ASSERT_NE(wrapped.memory(), nullptr);
    EXPECT_EQ(wrapped.memory()->get(), vmaAllocation);
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
        EXPECT_EQ(wrapped.memory(), nullptr);
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
    EXPECT_NE(buffer.memory(), nullptr);
    EXPECT_FALSE(other.created());
    EXPECT_EQ(other.memory(), nullptr);
}
}
