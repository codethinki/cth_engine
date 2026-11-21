#include "jvk/data/image.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/data/buffer.hpp"

#include <cstdint>
#include <vector>


namespace jvk {
using test::READBACK_CONFIG;
using test::STAGING_CONFIG;

namespace {
    constexpr VkExtent2D EXTENT{16, 16};

    constexpr cvk::Image::Config MIPPED_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .mipLevels = cvk::mip_level_count(EXTENT),
    };

    constexpr uint32_t WHITE = 0xffffffff;
}

JVK_TEST(Image, blit_mip_levels_records_its_accesses) {
    test::Context const context{};
    Image const image{context.core, MIPPED_CONFIG, EXTENT};
    Buffer<uint32_t> staging{context.core, STAGING_CONFIG, size_t{EXTENT.width} * EXTENT.height};
    staging.write(std::vector(size_t{EXTENT.width} * EXTENT.height, WHITE));
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, 1};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context), cvk::create};
    auto const lastLevel = image.mipLevels() - 1;

    primary.begin();
    primary.assume(image, {.layout = VK_IMAGE_LAYOUT_UNDEFINED, .scope = {}});
    image.copy(primary, staging);
    image.blitMipLevels(primary);
    readback.copy(primary, image, 0, lastLevel);
    primary.use(readback, BufferAccess{VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT});
    primary.end();

    for(uint32_t level = 0; level < image.mipLevels(); ++level) {
        auto const last = primary.states().last(image, level);
        ASSERT_TRUE(last.has_value());
        EXPECT_EQ(last->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }

    test::submit_and_wait(context, primary);

    auto const mapping = readback.map();
    readback.invalidate();
    EXPECT_EQ(mapping[0], WHITE);
}
}
