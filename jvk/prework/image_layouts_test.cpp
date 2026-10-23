#include "context.hpp"
#include "test.hpp"

#include "cvk/data/img/image.hpp"
#include "image_layouts.hpp"

#include <utility>


namespace cvk {
namespace {
    constexpr VkExtent2D EXTENT{16, 16};

    constexpr Image::Config IMAGE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .mipLevels = mip_level_count(EXTENT),
    };
}

CVK_TEST(ImageLayouts, unknown_image_is_undefined) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ImageLayouts const layouts{};

    EXPECT_TRUE(layouts.empty());
    EXPECT_FALSE(layouts.contains(image));
    EXPECT_TRUE(layouts.layouts(image).empty());
    EXPECT_EQ(layouts.layout(image, 2), VK_IMAGE_LAYOUT_UNDEFINED);
}

CVK_TEST(ImageLayouts, set_a_mip_range) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ImageLayouts layouts{};

    layouts.set(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 2);

    EXPECT_TRUE(layouts.contains(image));
    EXPECT_EQ(layouts.size(), 1);
    ASSERT_EQ(layouts.layouts(image).size(), image.mipLevels());
    EXPECT_EQ(layouts.layout(image, 0), VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(layouts.layout(image, 1), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(layouts.layout(image, 2), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(layouts.layout(image, 3), VK_IMAGE_LAYOUT_UNDEFINED);
}

CVK_TEST(ImageLayouts, set_the_remaining_mip_levels) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ImageLayouts layouts{};

    layouts.set(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    layouts.set(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2);

    EXPECT_EQ(layouts.layout(image, 1), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(layouts.layout(image, 2), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(layouts.layout(image, image.mipLevels() - 1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

CVK_TEST(ImageLayouts, follows_the_handle_when_the_image_moves) {
    test::Context const context{};
    Image image{context.core, IMAGE_CONFIG, EXTENT};
    ImageLayouts layouts{};
    layouts.set(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    Image const moved{std::move(image)};

    EXPECT_EQ(layouts.layout(moved), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

CVK_TEST(ImageLayouts, erase_and_clear) {
    test::Context const context{};
    Image const first{context.core, IMAGE_CONFIG, EXTENT};
    Image const second{context.core, IMAGE_CONFIG, EXTENT};
    ImageLayouts layouts{};
    layouts.set(first, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    layouts.set(second, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    layouts.erase(first);
    EXPECT_FALSE(layouts.contains(first));
    EXPECT_EQ(layouts.layout(first), VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_TRUE(layouts.contains(second));

    layouts.clear();
    EXPECT_TRUE(layouts.empty());
}
}
