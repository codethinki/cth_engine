#include "context.hpp"
#include "test.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <algorithm>


namespace cvk {
namespace {
    constexpr VkExtent2D EXTENT{16, 16};

    constexpr Image::Config IMAGE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .mipLevels = Image::evalMipLevelCount(EXTENT),
    };

    constexpr BarrierScope TRANSFER_WRITE{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    constexpr BarrierScope FRAGMENT_READ{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};

    bool all_layouts(Image const& image, VkImageLayout layout) {
        return std::ranges::all_of(image.layouts(), [layout](VkImageLayout l) { return l == layout; });
    }
}

CVK_TEST(Barrier, layout_transition_table) {
    constexpr auto toTransfer = layout_transition(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    static_assert(toTransfer.has_value());
    static_assert(toTransfer->src.access == VK_ACCESS_2_NONE);
    static_assert(toTransfer->dst.access == VK_ACCESS_2_TRANSFER_WRITE_BIT);

    constexpr auto toLocalRead = layout_transition(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR);
    static_assert(toLocalRead.has_value());
    static_assert((toLocalRead->dst.access & VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT) != 0);

    static_assert(!layout_transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED).has_value());
}

CVK_TEST(Barrier, layout_transition_info_covers_the_remaining_mip_levels) {
    constexpr auto info = ImageBarrierInfo::LayoutTransition(
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        BarrierScope{},
        TRANSFER_WRITE
    );

    static_assert(info.mipLevels == VK_REMAINING_MIP_LEVELS);
    static_assert(info.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

CVK_TEST(Barrier, memory_barriers_are_constexpr) {
    static_assert([] {
        Barrier barrier{VK_DEPENDENCY_BY_REGION_BIT};
        barrier.add(TRANSFER_WRITE, FRAGMENT_READ);
        bool const added = !barrier.empty();
        barrier.clear();
        return added && barrier.empty();
    }());
}

CVK_TEST(Barrier, add_and_clear) {
    test::Context const context{};
    BaseBuffer const buffer{context.core, {.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT}, 64};
    Barrier barrier{};
    EXPECT_TRUE(barrier.empty());

    barrier.add(TRANSFER_WRITE, FRAGMENT_READ);
    barrier.add(buffer, {.src = TRANSFER_WRITE, .dst = FRAGMENT_READ});
    EXPECT_FALSE(barrier.empty());

    barrier.clear();
    EXPECT_TRUE(barrier.empty());
}

CVK_TEST(Barrier, execute_memory_and_buffer_barriers) {
    test::Context const context{};
    BaseBuffer const buffer{context.core, {.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT}, 64};
    Barrier barrier{VK_DEPENDENCY_BY_REGION_BIT};
    barrier.add(TRANSFER_WRITE, FRAGMENT_READ);
    barrier.add(buffer, {.src = TRANSFER_WRITE, .dst = FRAGMENT_READ, .offset = 16, .size = 32});

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) { barrier.execute(primary); });

    EXPECT_EQ(barrier.flags(), VK_DEPENDENCY_BY_REGION_BIT);
}

CVK_TEST(Barrier, image_barrier_updates_the_layout_tracking) {
    test::Context const context{};
    Image image{context.core, IMAGE_CONFIG, EXTENT};

    Barrier barrier{};
    barrier.add(image, ImageBarrierInfo::LayoutTransition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {}, TRANSFER_WRITE));
    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) { barrier.execute(primary); });

    EXPECT_TRUE(all_layouts(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
}

CVK_TEST(Barrier, image_barrier_on_a_mip_range) {
    test::Context const context{};
    Image image{context.core, IMAGE_CONFIG, EXTENT};

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        image.transitionLayout(primary, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        image.transitionLayout(primary, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 2);
    });

    EXPECT_EQ(image.layout(0), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(image.layout(1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(image.layout(2), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(image.layout(3), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

CVK_TEST(Barrier, execute_twice_starts_from_the_new_layout) {
    test::Context const context{};
    Image image{context.core, IMAGE_CONFIG, EXTENT};
    Barrier barrier{};
    barrier.add(image, ImageBarrierInfo::LayoutTransition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {}, TRANSFER_WRITE));

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        barrier.execute(primary);
        barrier.execute(primary);
    });

    EXPECT_TRUE(all_layouts(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
}
}
