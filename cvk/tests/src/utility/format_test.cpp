#include "test.hpp"

#include "cvk/utility/format.hpp"

#include <cstdint>
#include <format>


namespace cvk {
CVK_TEST(format, vk_handle_names_the_handles) {
    static_assert(vk_handle<VkSemaphore>);
    static_assert(vk_handle<VkInstance>);
    static_assert(vk_handle<VmaAllocation>);
    static_assert(!vk_handle<int*>);
    static_assert(!vk_handle<void const*>);
}

CVK_TEST(format, vk_handle_formats_as_its_address) {
    auto const vkSemaphore = reinterpret_cast<VkSemaphore>(uintptr_t{0x1234});

    auto const address = std::format("{}", static_cast<void const*>(vkSemaphore));
    EXPECT_EQ(std::format("{}", vkSemaphore), std::format("cvk::vk_handle{{{}}}", address));
}
}
