#include "context.hpp"
#include "test.hpp"

#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"


namespace cvk {
namespace {
    constexpr VkExtent2D EXTENT{32, 32};

    constexpr Image::Config TEXTURE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        .mipLevels = mip_level_count(EXTENT),
    };
}

CVK_TEST(ImageView, config_range) {
    constexpr ImageView::Config config{.baseMipLevel = 2};
    constexpr auto range = config.range(VK_IMAGE_ASPECT_COLOR_BIT);

    static_assert(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
    static_assert(range.baseMipLevel == 2);
    static_assert(range.levelCount == VK_REMAINING_MIP_LEVELS);
    static_assert(range.baseArrayLayer == 0);
    static_assert(range.layerCount == VK_REMAINING_ARRAY_LAYERS);

    constexpr ImageView::Config depth{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT};
    static_assert(depth.range(VK_IMAGE_ASPECT_COLOR_BIT).aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT);
}

CVK_TEST(ImageView, view_type_follows_the_layers) {
    test::Context const context{};
    Image const flat{context.core, TEXTURE_CONFIG, EXTENT};
    auto layeredConfig = TEXTURE_CONFIG;
    layeredConfig.arrayLayers = 4;
    Image const layered{context.core, layeredConfig, EXTENT};
    Image const cube{context.core, Image::Config::CubeMap(VK_FORMAT_R8G8B8A8_UNORM), EXTENT};

    ImageView const flatView{flat, {}, create};
    ImageView const arrayView{layered, {}, create};
    ImageView const cubeView{cube, {}, create};
    ImageView const faceView{cube, {.baseArrayLayer = 2, .arrayLayers = 1}, create};
    ImageView const facesView{cube, {.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY}, create};

    EXPECT_EQ(flatView.viewType(), VK_IMAGE_VIEW_TYPE_2D);
    EXPECT_EQ(arrayView.viewType(), VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    EXPECT_EQ(arrayView.arrayLayers(), 4);
    EXPECT_EQ(cubeView.viewType(), VK_IMAGE_VIEW_TYPE_CUBE);
    EXPECT_EQ(faceView.viewType(), VK_IMAGE_VIEW_TYPE_2D);
    EXPECT_EQ(facesView.viewType(), VK_IMAGE_VIEW_TYPE_2D_ARRAY);
}

CVK_TEST(ImageView, base_constructor_does_not_create) {
    test::Context const context{};
    Image const image{context.core, TEXTURE_CONFIG, EXTENT};

    ImageView const view{image, {}};

    EXPECT_FALSE(view.created());
    EXPECT_EQ(&view.image(), &image);
    EXPECT_EQ(&view.core(), &context.core);
}

CVK_TEST(ImageView, create_and_destroy) {
    test::Context const context{};
    Image const image{context.core, TEXTURE_CONFIG, EXTENT};
    ImageView view{image, {}, create};

    ASSERT_TRUE(view.created());
    EXPECT_EQ(view.range().levelCount, VK_REMAINING_MIP_LEVELS);

    view.destroy();
    EXPECT_FALSE(view.created());
}

CVK_TEST(ImageView, create_on_a_mip_range) {
    test::Context const context{};
    Image const image{context.core, TEXTURE_CONFIG, EXTENT};

    ImageView const view{image, {.baseMipLevel = 1, .mipLevels = 2}, create};

    EXPECT_TRUE(view.created());
    EXPECT_EQ(view.range().baseMipLevel, 1);
    EXPECT_EQ(view.range().levelCount, 2);
}

CVK_TEST(ImageView, depth_view_takes_the_aspect_of_the_image) {
    test::Context const context{};
    Image const image{context.core, Image::Config::DepthBuffer(VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT), EXTENT};

    ImageView const view{image, {}, create};

    EXPECT_TRUE(view.created());
    EXPECT_EQ(view.range().aspectMask, VK_IMAGE_ASPECT_DEPTH_BIT);
}

CVK_TEST(ImageView, release_and_wrap) {
    test::Context const context{};
    Image const image{context.core, TEXTURE_CONFIG, EXTENT};
    ImageView view{image, {}, create};
    auto const vkView = view.get();

    auto const state = view.release();
    EXPECT_FALSE(view.created());

    ImageView const wrapped{image, {}, state};
    EXPECT_EQ(wrapped.get(), vkView);
}

CVK_TEST(ImageView, move_assignment_destroys_the_old_view) {
    test::Context const context{};
    Image const image{context.core, TEXTURE_CONFIG, EXTENT};
    ImageView view{image, {}, create};
    ImageView other{image, {.baseMipLevel = 1}, create};
    auto const vkView = other.get();

    view = std::move(other);

    EXPECT_EQ(view.get(), vkView);
    EXPECT_FALSE(other.created());
}
}
