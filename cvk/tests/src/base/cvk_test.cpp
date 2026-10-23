#include "test.hpp"

#include "cvk/base/cvk.hpp"

#include <volk.h>

#include <type_traits>


namespace cvk {
CVK_TEST(Cvk, is_not_copyable_or_movable) {
    static_assert(!std::is_copy_constructible_v<Cvk> && !std::is_copy_assignable_v<Cvk>);
    static_assert(!std::is_move_constructible_v<Cvk> && !std::is_move_assignable_v<Cvk>);
}

CVK_TEST(Cvk, nested_objects_keep_the_loader) {
    // the test environment holds a Cvk, the count never reaches 0 here
    ASSERT_TRUE(Cvk::initialized());
    {
        Cvk const outer{};
        {
            Cvk const inner{};
            EXPECT_TRUE(Cvk::initialized());
        }
        EXPECT_TRUE(Cvk::initialized());
    }
    EXPECT_TRUE(Cvk::initialized());

    uint32_t version = 0;
    ASSERT_NE(vkEnumerateInstanceVersion, nullptr);
    EXPECT_EQ(vkEnumerateInstanceVersion(&version), VK_SUCCESS);
}
}
