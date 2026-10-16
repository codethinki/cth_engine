#include "context.hpp"
#include "test.hpp"

#include "cvk/data/memory/memory.hpp"

#include <algorithm>
#include <ranges>


namespace cvk {
namespace {
    constexpr size_t SIZE = 1024;

    constexpr VkMemoryRequirements REQUIREMENTS{.size = SIZE, .alignment = 16, .memoryTypeBits = ~0u};

    /**
     * @brief host visible memory, memory without a buffer or an image cannot use VMA_MEMORY_USAGE_AUTO
     */
    constexpr Memory::Config HOST_CONFIG{
        .usage = VMA_MEMORY_USAGE_UNKNOWN,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
}

CVK_TEST(Memory, config_create_info) {
    constexpr Memory::Config config{
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_UNKNOWN,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        .preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
    };

    constexpr auto info = config.createInfo();

    EXPECT_EQ(info.flags, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    EXPECT_EQ(info.usage, VMA_MEMORY_USAGE_UNKNOWN);
    EXPECT_EQ(info.requiredFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    EXPECT_EQ(info.preferredFlags, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    EXPECT_EQ(info.pool, VK_NULL_HANDLE);
}

CVK_TEST(Memory, config_auto_usage) {
    static_assert(Memory::Config{}.autoUsage());
    static_assert(Memory::Config{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST}.autoUsage());
    static_assert(!HOST_CONFIG.autoUsage());
}

CVK_TEST(Memory, base_constructor_does_not_create) {
    test::Context const context{};

    Memory const memory{context.core, HOST_CONFIG};

    EXPECT_FALSE(memory.created());
}

CVK_TEST(Memory, create_and_destroy) {
    test::Context const context{};
    Memory memory{context.core, HOST_CONFIG, REQUIREMENTS};

    ASSERT_TRUE(memory.created());
    EXPECT_EQ(&memory.core(), &context.core);
    EXPECT_NE(memory.info().deviceMemory, VK_NULL_HANDLE);
    EXPECT_GE(memory.size(), SIZE);

    memory.destroy();
    EXPECT_FALSE(memory.created());
}

CVK_TEST(Memory, properties_have_the_required_flags) {
    test::Context const context{};
    Memory const memory{context.core, HOST_CONFIG, REQUIREMENTS};

    auto const properties = memory.properties();

    EXPECT_EQ(properties & HOST_CONFIG.requiredFlags, HOST_CONFIG.requiredFlags);
}

CVK_TEST(Memory, map_write_and_read) {
    test::Context const context{};
    Memory const memory{context.core, HOST_CONFIG, REQUIREMENTS};

    auto const written = memory.map();
    ASSERT_EQ(written.size(), memory.size());
    std::ranges::fill(written, 'x');
    memory.flush();
    memory.unmap();

    auto const read = memory.map();
    memory.invalidate();
    EXPECT_TRUE(std::ranges::all_of(read, [](char const c) { return c == 'x'; }));
    memory.unmap();
}

CVK_TEST(Memory, map_a_range) {
    test::Context const context{};
    Memory const memory{context.core, HOST_CONFIG, REQUIREMENTS};

    auto const whole = memory.map();
    auto const part = memory.map(16, 32);

    EXPECT_EQ(part.data(), whole.data() + 32);
    EXPECT_EQ(part.size(), 16);

    auto const end = memory.map(VK_WHOLE_SIZE, 32);
    EXPECT_EQ(end.size(), memory.size() - 32);

    memory.unmap();
    memory.unmap();
    memory.unmap();
}

CVK_TEST(Memory, release_and_wrap) {
    test::Context const context{};
    Memory memory{context.core, HOST_CONFIG, REQUIREMENTS};
    auto const vmaAllocation = memory.get();

    auto const state = memory.release();
    EXPECT_FALSE(memory.created());

    Memory const wrapped{context.core, HOST_CONFIG, state};
    EXPECT_EQ(wrapped.get(), vmaAllocation);
}

CVK_TEST(Memory, move_assignment_destroys_the_old_memory) {
    test::Context const context{};
    Memory memory{context.core, HOST_CONFIG, REQUIREMENTS};
    Memory other{context.core, HOST_CONFIG, REQUIREMENTS};
    auto const vmaAllocation = other.get();

    memory = std::move(other);

    EXPECT_EQ(memory.get(), vmaAllocation);
    EXPECT_FALSE(other.created());
}
}
