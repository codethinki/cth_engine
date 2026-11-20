#include "test.hpp"

#include "cvk/utility/vk_error_code.hpp"

#include <string>
#include <system_error>


namespace cvk {
CVK_TEST(vk_error_code, message_is_the_result_name) {
    auto const code = make_error_code(VK_ERROR_DEVICE_LOST);

    EXPECT_EQ(code.message(), std::string{"VK_ERROR_DEVICE_LOST"});
    EXPECT_STREQ(code.category().name(), "vulkan");
    EXPECT_EQ(code.value(), VK_ERROR_DEVICE_LOST);
    EXPECT_EQ(&code.category(), &vk_category());
}

CVK_TEST(vk_error_code, compares_by_result_and_category) {
    EXPECT_EQ(make_error_code(VK_ERROR_DEVICE_LOST), make_error_code(VK_ERROR_DEVICE_LOST));
    EXPECT_NE(make_error_code(VK_ERROR_DEVICE_LOST), make_error_code(VK_ERROR_OUT_OF_DATE_KHR));
    EXPECT_NE(
        make_error_code(VK_ERROR_DEVICE_LOST),
        std::error_code(VK_ERROR_DEVICE_LOST, std::generic_category())
    );
}

CVK_TEST(vk_error_code, success_is_no_error) {
    EXPECT_FALSE(make_error_code(VK_SUCCESS));
    EXPECT_TRUE(make_error_code(VK_TIMEOUT));
}
}
