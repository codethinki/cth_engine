#include "context.hpp"
#include "test.hpp"

#include "cvk/data/img/image.hpp"

#include <algorithm>


namespace cvk {
namespace {
    constexpr VkExtent2D EXTENT{64, 32};

    /**
     * @brief sampled texture with a full mip chain
     */
    constexpr Image::Config TEXTURE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .mipLevels = Image::evalMipLevelCount(EXTENT),
    };

    bool all_layouts(Image const& image, VkImageLayout layout) {
        return std::ranges::all_of(image.layouts(), [layout](VkImageLayout l) { return l == layout; });
    }
}

CVK_TEST(Image, eval_mip_level_count) {
    static_assert(Image::evalMipLevelCount({1, 1}) == 1);
    static_assert(Image::evalMipLevelCount({2, 1}) == 2);
    static_assert(Image::evalMipLevelCount({256, 256}) == 9);
    static_assert(Image::evalMipLevelCount({300, 200}) == 9);
    static_assert(Image::evalMipLevelCount({1, 1024}) == 11);
}

CVK_TEST(Image, config_create_info) {
    constexpr auto info = TEXTURE_CONFIG.createInfo(EXTENT);

    static_assert(info.imageType == VK_IMAGE_TYPE_2D);
    static_assert(info.extent.width == EXTENT.width && info.extent.height == EXTENT.height && info.extent.depth == 1);
    static_assert(info.mipLevels == 7);
    static_assert(info.arrayLayers == 1);
    static_assert(info.sharingMode == VK_SHARING_MODE_EXCLUSIVE);
    static_assert(info.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED);
}

CVK_TEST(Image, config_queue_families_are_concurrent) {
    static_assert([] {
        auto config = TEXTURE_CONFIG;
        config.queueFamilies = {0, 1};
        auto const info = config.createInfo(EXTENT);
        return info.sharingMode == VK_SHARING_MODE_CONCURRENT && info.queueFamilyIndexCount == 2;
    }());
}

CVK_TEST(Image, config_depth_buffer) {
    constexpr auto config = Image::Config::DepthBuffer(VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT);

    static_assert(config.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT);
    static_assert(config.usage == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    static_assert(config.memory.autoUsage());
    static_assert((Image::Config::debug_check(config), true));
}

CVK_TEST(Image, base_constructor_does_not_create) {
    test::Context const context{};

    Image const image{context.core, TEXTURE_CONFIG};

    EXPECT_FALSE(image.created());
    EXPECT_FALSE(image.memory().created());
    EXPECT_EQ(image.layouts().size(), TEXTURE_CONFIG.mipLevels);
    EXPECT_TRUE(all_layouts(image, VK_IMAGE_LAYOUT_UNDEFINED));
}

CVK_TEST(Image, create_and_destroy) {
    test::Context const context{};
    Image image{context.core, TEXTURE_CONFIG, EXTENT};

    ASSERT_TRUE(image.created());
    EXPECT_EQ(&image.core(), &context.core);
    EXPECT_EQ(image.extent().width, EXTENT.width);
    EXPECT_EQ(image.extent().height, EXTENT.height);
    EXPECT_EQ(image.mipLevels(), 7);
    EXPECT_TRUE(image.memory().created());
    EXPECT_TRUE(all_layouts(image, VK_IMAGE_LAYOUT_UNDEFINED));

    image.destroy();
    EXPECT_FALSE(image.created());
    EXPECT_FALSE(image.memory().created());
    EXPECT_EQ(image.extent().width, 0);
}

CVK_TEST(Image, create_depth_buffer) {
    test::Context const context{};

    Image const image{context.core, Image::Config::DepthBuffer(VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT), EXTENT};

    EXPECT_TRUE(image.created());
    EXPECT_EQ(image.aspectMask(), VK_IMAGE_ASPECT_DEPTH_BIT);
}

CVK_TEST(Image, release_and_wrap) {
    test::Context const context{};
    Image image{context.core, TEXTURE_CONFIG, EXTENT};
    auto const vkImage = image.get();
    auto const vmaAllocation = image.memory().get();

    auto const state = image.release();
    EXPECT_FALSE(image.created());
    EXPECT_FALSE(image.memory().created());
    ASSERT_TRUE(state.memory.has_value());
    EXPECT_EQ(state.levelLayouts.size(), TEXTURE_CONFIG.mipLevels);

    Image const wrapped{context.core, TEXTURE_CONFIG, state};
    EXPECT_EQ(wrapped.get(), vkImage);
    EXPECT_EQ(wrapped.memory().get(), vmaAllocation);
    EXPECT_EQ(wrapped.extent().width, EXTENT.width);
}

CVK_TEST(Image, wrap_fills_missing_layouts) {
    test::Context const context{};
    Image image{context.core, TEXTURE_CONFIG, EXTENT};
    auto state = image.release();
    state.levelLayouts = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};

    Image const wrapped{context.core, TEXTURE_CONFIG, state};

    ASSERT_EQ(wrapped.layouts().size(), TEXTURE_CONFIG.mipLevels);
    EXPECT_EQ(wrapped.layout(1), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(wrapped.layout(2), VK_IMAGE_LAYOUT_UNDEFINED);
}

CVK_TEST(Image, wrap_without_memory_destroys_only_the_image) {
    test::Context const context{};
    Image image{context.core, TEXTURE_CONFIG, EXTENT};
    auto state = image.release();

    // another owner frees the memory, after the image, like the swapchain
    Memory const memory{context.core, TEXTURE_CONFIG.memory, *state.memory};
    state.memory = std::nullopt;

    {
        Image const wrapped{context.core, TEXTURE_CONFIG, state};
        EXPECT_FALSE(wrapped.memory().created());
    }

    EXPECT_TRUE(memory.created());
}

CVK_TEST(Image, move_assignment_destroys_the_old_image) {
    test::Context const context{};
    Image image{context.core, TEXTURE_CONFIG, EXTENT};
    Image other{context.core, TEXTURE_CONFIG, EXTENT};
    auto const vkImage = other.get();

    image = std::move(other);

    EXPECT_EQ(image.get(), vkImage);
    EXPECT_TRUE(image.memory().created());
    EXPECT_FALSE(other.created());
}
}
