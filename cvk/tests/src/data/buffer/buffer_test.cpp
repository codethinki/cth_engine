#include "context.hpp"
#include "test.hpp"

#include "cvk/data/buffer/buffer.hpp"

#include <algorithm>
#include <array>
#include <string>


namespace cvk {
namespace {
    struct Vertex {
        float x, y, z;
        uint32_t color;

        friend constexpr bool operator==(Vertex const&, Vertex const&) = default;
    };

    constexpr size_t COUNT = 8;

    constexpr std::array<Vertex, 3> TRIANGLE{
        Vertex{0.f, 1.f, 0.f, 0xff0000ff},
        Vertex{1.f, 0.f, 0.f, 0x00ff00ff},
        Vertex{0.f, 0.f, 1.f, 0x0000ffff},
    };

    /**
     * @brief the cpu writes and reads
     */
    constexpr Buffer<Vertex>::Config HOST_CONFIG{
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT},
    };

    /**
     * @brief Buffer<T> is a valid type
     */
    template<class T>
    concept valid_buffer = requires { sizeof(Buffer<T>); };

    void take_base(BaseBuffer const& buffer, size_t size) { EXPECT_EQ(buffer.size(), size); }
}

CVK_TEST(Buffer, elements_must_be_trivially_copyable) {
    static_assert(valid_buffer<Vertex>);
    static_assert(valid_buffer<char>);
    static_assert(!valid_buffer<std::string>);
}

CVK_TEST(Buffer, adds_no_members) { static_assert(sizeof(Buffer<Vertex>) == sizeof(BaseBuffer)); }

CVK_TEST(Buffer, write_to_mapped_elements_is_constexpr) {
    static_assert([] {
        std::array<Vertex, 4> mapped{};
        Buffer<Vertex>::write(TRIANGLE, mapped);
        return mapped[2] == TRIANGLE[2] && mapped[3] == Vertex{};
    }());
}

CVK_TEST(Buffer, create_counts_elements) {
    test::Context const context{};

    Buffer<Vertex> const buffer{context.core, HOST_CONFIG, COUNT};

    ASSERT_TRUE(buffer.created());
    EXPECT_EQ(buffer.elements(), COUNT);
    EXPECT_EQ(buffer.size(), COUNT * sizeof(Vertex));
    take_base(buffer, COUNT * sizeof(Vertex));
}

CVK_TEST(Buffer, write_and_map) {
    test::Context const context{};
    Buffer<Vertex> const buffer{context.core, HOST_CONFIG, COUNT};

    buffer.write(TRIANGLE, 2);

    auto const mapped = buffer.map(TRIANGLE.size(), 2);
    buffer.invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, TRIANGLE));
    buffer.unmap();
}

CVK_TEST(Buffer, map_whole_buffer) {
    test::Context const context{};
    Buffer<Vertex> const buffer{context.core, HOST_CONFIG, COUNT};

    auto const whole = buffer.map();
    EXPECT_EQ(whole.size(), COUNT);

    Buffer<Vertex>::write(TRIANGLE, whole);
    buffer.flush();
    buffer.flush(1, 2);

    auto const end = buffer.map(VK_WHOLE_SIZE, 5);
    EXPECT_EQ(end.size(), COUNT - 5);
    EXPECT_EQ(end.data(), whole.data() + 5);

    buffer.unmap();
    buffer.unmap();
}

CVK_TEST(Buffer, descriptor_info_in_elements) {
    test::Context const context{};
    Buffer<Vertex> const buffer{context.core, HOST_CONFIG, COUNT};

    auto const whole = buffer.descriptorInfo();
    EXPECT_EQ(whole.offset, 0);
    EXPECT_EQ(whole.range, COUNT * sizeof(Vertex));

    auto const end = buffer.descriptorInfo(VK_WHOLE_SIZE, 3);
    EXPECT_EQ(end.offset, 3 * sizeof(Vertex));
    EXPECT_EQ(end.range, (COUNT - 3) * sizeof(Vertex));

    auto const part = buffer.descriptorInfo(2, 1);
    EXPECT_EQ(part.offset, sizeof(Vertex));
    EXPECT_EQ(part.range, 2 * sizeof(Vertex));
}

CVK_TEST(Buffer, release_and_wrap_as_bytes) {
    test::Context const context{};
    Buffer<Vertex> buffer{context.core, HOST_CONFIG, COUNT};
    buffer.write(TRIANGLE);
    auto const vkBuffer = buffer.get();

    auto const state = buffer.release();
    EXPECT_FALSE(buffer.created());

    Buffer<char> const bytes{context.core, HOST_CONFIG, state};
    EXPECT_EQ(bytes.get(), vkBuffer);
    EXPECT_EQ(bytes.elements(), COUNT * sizeof(Vertex));
}

CVK_TEST(Buffer, move_assignment_destroys_the_old_buffer) {
    test::Context const context{};
    Buffer<Vertex> buffer{context.core, HOST_CONFIG, COUNT};
    Buffer<Vertex> other{context.core, HOST_CONFIG, 2 * COUNT};
    auto const vkBuffer = other.get();

    buffer = std::move(other);

    EXPECT_EQ(buffer.get(), vkBuffer);
    EXPECT_EQ(buffer.elements(), 2 * COUNT);
    EXPECT_FALSE(other.created());
}
}
