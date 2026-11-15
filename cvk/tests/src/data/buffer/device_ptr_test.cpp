#include "test.hpp"

#include "cvk/data/buffer/device_ptr.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>


namespace cvk {
namespace {
    struct Vertex {
        float x, y, z;
        uint32_t color;
    };

    constexpr VkDeviceAddress ADDRESS = 0x1000;

    /**
     * push data with a device address, an aggregate
     */
    struct PushData {
        device_ptr<Vertex> vertices;
        uint32_t count;
    };
}

CVK_TEST(device_ptr, default_is_null) {
    static_assert(device_ptr<Vertex>{}.get() == 0);
    static_assert(device_ptr<Vertex>{} == device_ptr<Vertex>{0});
}

CVK_TEST(device_ptr, fits_push_data) {
    static_assert(sizeof(device_ptr<Vertex>) == 8);
    static_assert(std::is_trivially_copyable_v<device_ptr<Vertex>>);
    static_assert(std::is_aggregate_v<PushData>);
    static_assert(PushData{}.vertices.get() == 0);
}

CVK_TEST(device_ptr, arithmetic_steps_in_elements) {
    constexpr device_ptr<Vertex> ptr{ADDRESS};

    static_assert((ptr + 3).get() == ADDRESS + 3 * sizeof(Vertex));
    static_assert((3 + ptr).get() == ADDRESS + 3 * sizeof(Vertex));
    static_assert((ptr - 2).get() == ADDRESS - 2 * sizeof(Vertex));
    static_assert((ptr + -2).get() == ADDRESS - 2 * sizeof(Vertex));
    static_assert([] {
        device_ptr<Vertex> moving{ADDRESS};
        moving += 5;
        moving -= 2;
        return moving.get() == ADDRESS + 3 * sizeof(Vertex);
    }());
    static_assert(device_ptr<std::byte>{ADDRESS} + 3 == device_ptr<std::byte>{ADDRESS + 3});
}

CVK_TEST(device_ptr, difference_in_elements) {
    constexpr device_ptr<Vertex> ptr{ADDRESS};

    static_assert((ptr + 7) - ptr == 7);
    static_assert(ptr - (ptr + 7) == -7);
    static_assert(ptr - ptr == 0);
}

CVK_TEST(device_ptr, compares_the_addresses) {
    constexpr device_ptr<Vertex> ptr{ADDRESS};

    static_assert(ptr == device_ptr<Vertex>{ADDRESS});
    static_assert(ptr != ptr + 1);
    static_assert(ptr < ptr + 1);
    static_assert(ptr + 1 > ptr);
    static_assert(ptr <= ptr);
}
}
