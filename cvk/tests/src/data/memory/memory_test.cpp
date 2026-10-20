#include "context.hpp"
#include "test.hpp"

#include "cvk/data/memory/memory.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

    {
        auto const written = memory.map();
        ASSERT_EQ(written.size(), memory.size());
        std::ranges::fill(written, std::byte{'x'});
        memory.flush();
    }

    auto const read = memory.map();
    memory.invalidate();
    EXPECT_TRUE(std::ranges::all_of(read, [](std::byte b) { return b == std::byte{'x'}; }));
}

CVK_TEST(Memory, map_a_range) {
    test::Context const context{};
    Memory const memory{context.core, HOST_CONFIG, REQUIREMENTS};

    auto const whole = memory.map();
    auto const part = memory.map({.offset = 32, .size = 16});
    auto const end = memory.map({.offset = 32});

    EXPECT_EQ(part.data(), whole.data() + 32);
    EXPECT_EQ(part.size(), 16);
    EXPECT_EQ(end.size(), memory.size() - 32);
}

CVK_TEST(Memory, write_copies_the_data) {
    test::Context const context{};
    Memory const memory{context.core, HOST_CONFIG, REQUIREMENTS};
    std::array<std::byte, 16> data{};
    std::ranges::fill(data, std::byte{'y'});

    memory.write(data, 32);

    auto const mapped = memory.map({.offset = 32, .size = data.size()});
    EXPECT_TRUE(std::ranges::equal(mapped, data));
}

CVK_TEST(Memory, the_mapping_unmaps) {
    test::Context const context{};
    Memory const memory{context.core, HOST_CONFIG, REQUIREMENTS};

    EXPECT_TRUE(memory.mapped().empty());
    {
        auto const mapping = memory.map();
        EXPECT_EQ(memory.mapped().data(), mapping.data());
        EXPECT_EQ(memory.mapped().size(), memory.size());
    }
    EXPECT_TRUE(memory.mapped().empty());
}

CVK_TEST(Memory, a_moved_mapping_unmaps_once) {
    test::Context const context{};
    Memory const memory{context.core, HOST_CONFIG, REQUIREMENTS};

    {
        auto mapping = memory.map();
        Mapping<std::byte> const moved{std::move(mapping)};
        EXPECT_FALSE(memory.mapped().empty());
    }
    EXPECT_TRUE(memory.mapped().empty());
}

CVK_TEST(Memory, mapping_as_elements) {
    test::Context const context{};
    Memory const memory{context.core, HOST_CONFIG, REQUIREMENTS};

    {
        auto bytes = memory.map({.offset = 0, .size = 64});
        auto const* data = bytes.data();
        auto const elements = std::move(bytes).as<uint32_t>();

        EXPECT_EQ(elements.size(), 16);
        EXPECT_EQ(static_cast<void const*>(elements.data()), static_cast<void const*>(data));
    }
    EXPECT_TRUE(memory.mapped().empty());
}

CVK_TEST(Memory, mapped_bit_maps_persistently) {
    test::Context const context{};
    auto config = HOST_CONFIG;
    config.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

    Memory const memory{context.core, config, REQUIREMENTS};

    EXPECT_FALSE(memory.mapped().empty());
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
