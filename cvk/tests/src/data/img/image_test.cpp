#include "context.hpp"
#include "test.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/buffer/buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>


namespace cvk {
namespace {
    using test::READBACK_CONFIG;
    using test::STAGING_CONFIG;

    constexpr VkExtent2D EXTENT{64, 32};

    /**
     * @brief sampled texture with a full mip chain
     */
    constexpr Image::Config TEXTURE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .mipLevels = mip_level_count(EXTENT),
    };

    /**
     * @brief image that the gpu copies into and out of, two mip levels
     */
    constexpr Image::Config COPY_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .mipLevels = 2,
    };

    /**
     * @brief image that fills its mip chain with blits
     */
    constexpr Image::Config BLIT_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .mipLevels = mip_level_count(EXTENT),
    };
}

CVK_TEST(Image, mip_level_count) {
    static_assert(mip_level_count({1, 1}) == 1);
    static_assert(mip_level_count({2, 1}) == 2);
    static_assert(mip_level_count({256, 256}) == 9);
    static_assert(mip_level_count({300, 200}) == 9);
    static_assert(mip_level_count({1, 1024}) == 11);
}

CVK_TEST(Image, mip_extent) {
    static_assert(mip_extent({1, 1}, 0) == VkExtent2D{1, 1});
    static_assert(mip_extent({1, 1}, 3) == VkExtent2D{1, 1});
    static_assert(mip_extent({256, 256}, 0) == VkExtent2D{256, 256});
    static_assert(mip_extent({256, 256}, 8) == VkExtent2D{1, 1});
    static_assert(mip_extent({300, 200}, 1) == VkExtent2D{150, 100});
    static_assert(mip_extent({300, 200}, 3) == VkExtent2D{37, 25});
    static_assert(mip_extent({1, 1024}, 4) == VkExtent2D{1, 64});
    static_assert(mip_extent({1024, 4}, 4) == VkExtent2D{64, 1});

    // levels beyond the count, also beyond the bit count of uint32_t
    static_assert(mip_extent({300, 200}, mip_level_count({300, 200})) == VkExtent2D{1, 1});
    static_assert(mip_extent({4096, 4096}, 32) == VkExtent2D{1, 1});
    static_assert(mip_extent({4096, 4096}, 100) == VkExtent2D{1, 1});
}

CVK_TEST(Image, format_aspects) {
    static_assert(format_aspects(VK_FORMAT_R8G8B8A8_UNORM) == VK_IMAGE_ASPECT_COLOR_BIT);
    static_assert(format_aspects(VK_FORMAT_D32_SFLOAT) == VK_IMAGE_ASPECT_DEPTH_BIT);
    static_assert(format_aspects(VK_FORMAT_S8_UINT) == VK_IMAGE_ASPECT_STENCIL_BIT);
    static_assert(
        format_aspects(VK_FORMAT_D24_UNORM_S8_UINT) == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
    );
}

CVK_TEST(Image, config_aspect_mask_fits_the_format) {
    static_assert([] {
        Image::Config const config{.format = VK_FORMAT_D24_UNORM_S8_UINT, .usage = VK_IMAGE_USAGE_SAMPLED_BIT};
        Image::Config::debug_check(config);
        return true;
    }());
    constexpr auto depthBuffer = Image::Config::DepthBuffer(VK_FORMAT_D24_UNORM_S8_UINT, VK_SAMPLE_COUNT_1_BIT);
    static_assert((Image::Config::debug_check(depthBuffer), true));
}

CVK_TEST(Image, aspect_mask_comes_from_the_format) {
    test::Context const context{};

    Image const color{context.core, {.format = VK_FORMAT_R8G8B8A8_UNORM, .usage = VK_IMAGE_USAGE_SAMPLED_BIT}};
    Image const depth{
        context.core,
        {.format = VK_FORMAT_D16_UNORM, .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}
    };

    EXPECT_EQ(color.aspectMask(), VK_IMAGE_ASPECT_COLOR_BIT);
    EXPECT_EQ(depth.aspectMask(), VK_IMAGE_ASPECT_DEPTH_BIT);
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

CVK_TEST(Image, config_cube_map) {
    constexpr auto config = Image::Config::CubeMap(VK_FORMAT_R8G8B8A8_UNORM, 3);
    static_assert(config.arrayLayers == 6);
    static_assert((config.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0);
    static_assert((config.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0);
    static_assert((Image::Config::CubeMap(VK_FORMAT_R8G8B8A8_UNORM).usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0);
    static_assert(config.createInfo({8, 8}).arrayLayers == 6);
    static_assert((Image::Config::debug_check(config), true));
}

CVK_TEST(Image, base_constructor_does_not_create) {
    test::Context const context{};

    Image const image{context.core, TEXTURE_CONFIG};

    EXPECT_FALSE(image.created());
    EXPECT_EQ(image.memory(), nullptr);
}

CVK_TEST(Image, create_and_destroy) {
    test::Context const context{};
    Image image{context.core, TEXTURE_CONFIG, EXTENT};

    ASSERT_TRUE(image.created());
    EXPECT_EQ(&image.core(), &context.core);
    EXPECT_EQ(image.extent().width, EXTENT.width);
    EXPECT_EQ(image.extent().height, EXTENT.height);
    EXPECT_EQ(image.mipLevels(), 7);
    EXPECT_NE(image.memory(), nullptr);

    image.destroy();
    EXPECT_FALSE(image.created());
    EXPECT_EQ(image.memory(), nullptr);
    EXPECT_EQ(image.extent().width, 0);
}

CVK_TEST(Image, create_depth_buffer) {
    test::Context const context{};

    Image const image{context.core, Image::Config::DepthBuffer(VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT), EXTENT};

    EXPECT_TRUE(image.created());
    EXPECT_EQ(image.aspectMask(), VK_IMAGE_ASPECT_DEPTH_BIT);
}

CVK_TEST(Image, copy_from_a_buffer_into_two_mip_levels) {
    test::Context const context{};
    Image const image{context.core, COPY_CONFIG, EXTENT};

    size_t const mip0 = size_t{EXTENT.width} * EXTENT.height * 4;
    size_t const mip1 = mip0 / 4;
    BaseBuffer const staging{context.core, STAGING_CONFIG, mip0 + mip1};
    BaseBuffer const readback{context.core, READBACK_CONFIG, mip0 + mip1};

    std::vector<std::byte> pixels(mip0 + mip1);
    for(size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = static_cast<std::byte>(i % 251);
    staging.write(pixels);

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        Barrier toDst{};
        toDst.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toDst.execute(primary);

        image.copy(primary, staging, 0, 0);
        image.copy(primary, staging, mip0, 1);

        Barrier toSrc{};
        toSrc.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        toSrc.execute(primary);
        test::copy_to_buffer(context, primary, image, readback, 0, 0);
        test::copy_to_buffer(context, primary, image, readback, mip0, 1);
        test::host_read_barrier(context, primary);
    });

    auto const mapped = readback.map({.size = pixels.size()});
    readback.invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, pixels));
}

CVK_TEST(Image, copy_two_mip_levels_to_a_buffer) {
    test::Context const context{};
    Image const image{context.core, COPY_CONFIG, EXTENT};

    size_t const mip0 = size_t{EXTENT.width} * EXTENT.height * 4;
    size_t const mip1 = mip0 / 4;
    BaseBuffer const staging{context.core, STAGING_CONFIG, mip0 + mip1};
    BaseBuffer const readback{context.core, READBACK_CONFIG, mip0 + mip1};

    std::vector<std::byte> pixels(mip0 + mip1);
    for(size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = static_cast<std::byte>(i % 251);
    staging.write(pixels);

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        Barrier toDst{};
        toDst.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toDst.execute(primary);

        image.copy(primary, staging, 0, 0);
        image.copy(primary, staging, mip0, 1);

        Barrier toSrc{};
        toSrc.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        toSrc.execute(primary);
        image.copyTo(primary, readback, 0, 0);
        readback.copy(primary, image, mip0, 1);
        test::host_read_barrier(context, primary);
    });

    auto const mapped = readback.map({.size = pixels.size()});
    readback.invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, pixels));
}

CVK_TEST(Image, typed_buffer_copies_a_mip_level_in_elements) {
    test::Context const context{};
    Image const image{context.core, COPY_CONFIG, EXTENT};

    // one R8G8B8A8 texel per element
    size_t const mip1 = size_t{EXTENT.width / 2} * (EXTENT.height / 2);
    BaseBuffer const staging{context.core, STAGING_CONFIG, mip1 * sizeof(uint32_t)};
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, mip1 + 1};

    std::vector<uint32_t> texels(mip1);
    for(size_t i = 0; i < texels.size(); ++i)
        texels[i] = static_cast<uint32_t>(i * 2654435761u);
    staging.write(std::as_bytes(std::span{texels}));

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        Barrier toDst{};
        toDst.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toDst.execute(primary);
        image.copy(primary, staging, 0, 1);

        Barrier toSrc{};
        toSrc.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        toSrc.execute(primary);
        readback.copy(primary, image, 1, 1);
        test::host_read_barrier(context, primary);
    });

    auto const mapped = readback.map({.offset = 1});
    readback.invalidate();
    EXPECT_TRUE(std::ranges::equal(mapped, texels));
}

CVK_TEST(Image, blit_mip_levels_fills_the_mip_chain) {
    test::Context const context{};
    Image const image{context.core, BLIT_CONFIG, EXTENT};

    // one color: linear blits keep it in every level
    constexpr uint32_t color = 0x80'40'20'10;
    std::vector<uint32_t> const texels(size_t{EXTENT.width} * EXTENT.height, color);
    BaseBuffer const staging{context.core, STAGING_CONFIG, texels.size() * sizeof(uint32_t)};
    staging.write(std::as_bytes(std::span{texels}));

    uint32_t const last = BLIT_CONFIG.mipLevels - 1;
    size_t const level2 = size_t{EXTENT.width >> 2} * (EXTENT.height >> 2);
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, level2 + 1};

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        Barrier toDst{};
        toDst.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toDst.execute(primary);
        image.copy(primary, staging);

        image.blitMipLevels(primary);

        // every level is in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        readback.copy(primary, image, 0, 2);
        readback.copy(primary, image, level2, last);
        test::host_read_barrier(context, primary);
    });

    auto const mapped = readback.map();
    readback.invalidate();
    EXPECT_TRUE(std::ranges::all_of(mapped, [](uint32_t texel) { return texel == color; }));
}

CVK_TEST(Image, cube_map_copies_and_blits_every_face) {
    test::Context const context{};
    constexpr VkExtent2D extent{8, 8};
    Image const image{
        context.core,
        Image::Config::CubeMap(VK_FORMAT_R8G8B8A8_UNORM, mip_level_count(extent)),
        extent
    };

    // one color per face, linear blits keep it, the faces follow each other in the buffer
    auto const color = [](uint32_t face) { return 0x01'01'01'01u * (face + 1) * 30; };
    size_t const faceTexels = size_t{extent.width} * extent.height;
    std::vector<uint32_t> texels(6 * faceTexels);
    for(uint32_t face = 0; face < 6; ++face)
        std::ranges::fill(std::span{texels}.subspan(face * faceTexels, faceTexels), color(face));

    BaseBuffer const staging{context.core, STAGING_CONFIG, texels.size() * sizeof(uint32_t)};
    staging.write(std::as_bytes(std::span{texels}));
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, 6};

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        Barrier toDst{};
        toDst.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toDst.execute(primary);

        image.copy(primary, staging);
        image.blitMipLevels(primary);

        // the last level has one texel per face
        image.copyTo(primary, readback, 0, image.mipLevels() - 1);
        test::host_read_barrier(context, primary);
    });

    auto const mapped = readback.map();
    readback.invalidate();
    for(uint32_t face = 0; face < 6; ++face)
        EXPECT_EQ(mapped[face], color(face));
}

CVK_TEST(Image, release_and_wrap) {
    test::Context const context{};
    Image image{context.core, TEXTURE_CONFIG, EXTENT};
    auto const vkImage = image.get();
    auto const vmaAllocation = image.memory()->get();

    auto const state = image.release();
    EXPECT_FALSE(image.created());
    EXPECT_EQ(image.memory(), nullptr);
    ASSERT_TRUE(state.memory.has_value());

    Image const wrapped{context.core, TEXTURE_CONFIG, state};
    EXPECT_EQ(wrapped.get(), vkImage);
    EXPECT_EQ(wrapped.memory()->get(), vmaAllocation);
    EXPECT_EQ(wrapped.extent().width, EXTENT.width);
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
        EXPECT_EQ(wrapped.memory(), nullptr);
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
    EXPECT_NE(image.memory(), nullptr);
    EXPECT_FALSE(other.created());
}
}
