#include "jvk/sync/resource_states.hpp"

#include "context.hpp"
#include "test.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <utility>


namespace jvk {
using cvk::BarrierScope;
using cvk::BaseBuffer;
using cvk::Image;

namespace {
    constexpr VkExtent2D EXTENT{16, 16};
    constexpr size_t SIZE = 256;

    constexpr Image::Config IMAGE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .mipLevels = cvk::mip_level_count(EXTENT),
    };

    constexpr BaseBuffer::Config BUFFER_CONFIG{.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};

    constexpr ImageState UNDEFINED{.layout = VK_IMAGE_LAYOUT_UNDEFINED, .scope = {}};

    constexpr ImageAccess FRAGMENT_READ{
        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
    };

    constexpr ImageAccess COMPUTE_READ{
        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
    };

    constexpr ImageAccess TRANSFER_WRITE{
        .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    };

    constexpr BufferAccess SHADER_READ{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT};
    constexpr BufferAccess SHADER_WRITE{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};

    [[nodiscard]] constexpr ImageAccess with_mips(ImageAccess access, uint32_t firstMipLevel, uint32_t mipLevels) {
        access.firstMipLevel = firstMipLevel;
        access.mipLevels = mipLevels;
        return access;
    }
}

JVK_TEST(ResourceStates, unknown_image_has_no_state) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates const states{};

    EXPECT_TRUE(states.empty());
    EXPECT_FALSE(states.contains(image));
    EXPECT_FALSE(states.first(image, 2).has_value());
    EXPECT_FALSE(states.last(image, 2).has_value());
}

JVK_TEST(ResourceStates, first_access_is_a_first_use) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    cvk::Barrier barrier{};

    states.access(image, FRAGMENT_READ, barrier);

    EXPECT_TRUE(barrier.empty());
    EXPECT_EQ(states.size(), image.mipLevels());
    ASSERT_TRUE(states.first(image, 2).has_value());
    EXPECT_EQ(states.first(image, 2)->layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(states.last(image, 2)->scope.stages, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    EXPECT_FALSE(states.discards(image, 2));
}

JVK_TEST(ResourceStates, known_state_gets_a_barrier) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    cvk::Barrier barrier{};
    states.assume(image, UNDEFINED);

    states.access(image, with_mips(TRANSFER_WRITE, 1, 2), barrier);

    // one entry for both mip levels
    ASSERT_EQ(barrier.images.size(), 1);
    auto const& vkBarrier = barrier.images[0];
    EXPECT_EQ(vkBarrier.image, image.get());
    EXPECT_EQ(vkBarrier.oldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(vkBarrier.newLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(vkBarrier.subresourceRange.baseMipLevel, 1);
    EXPECT_EQ(vkBarrier.subresourceRange.levelCount, 2);
    EXPECT_EQ(vkBarrier.subresourceRange.aspectMask, image.aspectMask());
    EXPECT_EQ(vkBarrier.dstAccessMask, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    // the assume came before the first access
    EXPECT_FALSE(states.first(image, 1).has_value());
    EXPECT_EQ(states.last(image, 1)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(states.last(image, 0)->layout, VK_IMAGE_LAYOUT_UNDEFINED);
}

JVK_TEST(ResourceStates, read_after_read_needs_no_barrier) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    cvk::Barrier barrier{};

    states.access(image, FRAGMENT_READ, barrier);
    states.access(image, COMPUTE_READ, barrier);

    EXPECT_TRUE(barrier.empty());
    constexpr auto stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    EXPECT_EQ(states.last(image, 0)->scope.stages, stages);
    // the first use spans the accesses until the first barrier
    EXPECT_EQ(states.first(image, 0)->scope.stages, stages);
}

JVK_TEST(ResourceStates, write_after_write_gets_a_barrier) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    cvk::Barrier barrier{};

    states.access(image, with_mips(TRANSFER_WRITE, 0, 1), barrier);
    states.access(image, with_mips(TRANSFER_WRITE, 0, 1), barrier);

    ASSERT_EQ(barrier.images.size(), 1);
    auto const& vkBarrier = barrier.images[0];
    EXPECT_EQ(vkBarrier.oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(vkBarrier.newLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(vkBarrier.srcAccessMask, VK_ACCESS_2_TRANSFER_WRITE_BIT);
}

JVK_TEST(ResourceStates, a_barrier_ends_the_first_use) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    cvk::Barrier barrier{};

    states.access(image, FRAGMENT_READ, barrier);
    states.access(image, TRANSFER_WRITE, barrier);
    states.access(image, with_mips(TRANSFER_WRITE, 0, 1), barrier);

    EXPECT_EQ(states.first(image, 0)->layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(states.first(image, 0)->scope.stages, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(states.last(image, 0)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

JVK_TEST(ResourceStates, read_after_read_only_makes_writes_available) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    cvk::Barrier barrier{};

    states.access(image, FRAGMENT_READ, barrier);
    states.access(image, TRANSFER_WRITE, barrier);

    ASSERT_EQ(barrier.images.size(), 1);
    EXPECT_EQ(barrier.images[0].srcStageMask, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(barrier.images[0].srcAccessMask, VK_ACCESS_2_NONE);
}

JVK_TEST(ResourceStates, discard_comes_from_undefined) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    cvk::Barrier barrier{};
    auto discard = TRANSFER_WRITE;
    discard.discard = true;

    states.access(image, with_mips(discard, 0, 1), barrier);
    EXPECT_TRUE(barrier.empty());
    EXPECT_TRUE(states.discards(image, 0));

    states.access(image, with_mips(FRAGMENT_READ, 0, 1), barrier);
    states.access(image, with_mips(discard, 0, 1), barrier);

    ASSERT_EQ(barrier.images.size(), 2);
    EXPECT_EQ(barrier.images[1].oldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(barrier.images[1].newLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

JVK_TEST(ResourceStates, assume_a_mip_range) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};

    states.assume(image, {.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .scope = {}}, 1, 2);

    EXPECT_TRUE(states.contains(image));
    EXPECT_EQ(states.size(), 2);
    EXPECT_FALSE(states.last(image, 0).has_value());
    EXPECT_EQ(states.last(image, 1)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(states.last(image, 2)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_FALSE(states.last(image, 3).has_value());
}

JVK_TEST(ResourceStates, assume_the_remaining_mip_levels) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};

    states.assume(image, {.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .scope = {}});
    states.assume(image, {.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .scope = {}}, 2);

    EXPECT_EQ(states.last(image, 1)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(states.last(image, 2)->layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(states.last(image, image.mipLevels() - 1)->layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

JVK_TEST(ResourceStates, assume_keeps_the_first_use) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    cvk::Barrier barrier{};

    states.access(image, FRAGMENT_READ, barrier);
    states.assume(
        image,
        {
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .scope = {.stages = FRAGMENT_READ.stages, .access = FRAGMENT_READ.access},
        }
    );
    states.access(image, COMPUTE_READ, barrier);

    EXPECT_TRUE(barrier.empty());
    EXPECT_EQ(states.first(image, 0)->scope.stages, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(
        states.last(image, 0)->scope.stages,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    );
}

JVK_TEST(ResourceStates, buffer_barriers) {
    test::Context const context{};
    BaseBuffer const buffer{context.core, BUFFER_CONFIG, SIZE};
    ResourceStates states{};
    cvk::Barrier barrier{};

    states.access(buffer, SHADER_WRITE, barrier);
    EXPECT_TRUE(barrier.empty());
    EXPECT_EQ(states.first(buffer)->scope.access, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

    states.access(buffer, SHADER_READ, barrier);
    states.access(buffer, SHADER_READ, barrier);

    ASSERT_EQ(barrier.buffers.size(), 1);
    auto const& vkBarrier = barrier.buffers[0];
    EXPECT_EQ(vkBarrier.buffer, buffer.get());
    EXPECT_EQ(vkBarrier.srcAccessMask, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    EXPECT_EQ(vkBarrier.dstAccessMask, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    EXPECT_EQ(states.last(buffer)->scope.access, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
}

JVK_TEST(ResourceStates, assume_a_buffer) {
    test::Context const context{};
    BaseBuffer const buffer{context.core, BUFFER_CONFIG, SIZE};
    ResourceStates states{};
    cvk::Barrier barrier{};

    states.assume(buffer, BufferState{.scope = {.stages = SHADER_WRITE.stages, .access = SHADER_WRITE.access}});
    states.access(buffer, SHADER_READ, barrier);

    EXPECT_FALSE(states.first(buffer).has_value());
    ASSERT_EQ(barrier.buffers.size(), 1);
    EXPECT_EQ(barrier.buffers[0].srcAccessMask, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}

JVK_TEST(ResourceStates, append_gets_barriers_for_the_first_uses_of_next) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    Image const other{context.core, IMAGE_CONFIG, EXTENT};
    BaseBuffer const buffer{context.core, BUFFER_CONFIG, SIZE};
    ResourceStates states{};
    ResourceStates next{};
    cvk::Barrier barrier{};
    states.assume(image, UNDEFINED);
    states.access(buffer, SHADER_WRITE, barrier);

    next.access(image, with_mips(TRANSFER_WRITE, 0, 1), barrier);
    next.access(other, FRAGMENT_READ, barrier);
    next.access(buffer, SHADER_READ, barrier);
    ASSERT_TRUE(barrier.empty());

    states.append(next, barrier);

    ASSERT_EQ(barrier.images.size(), 1);
    EXPECT_EQ(barrier.images[0].oldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(barrier.images[0].newLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    ASSERT_EQ(barrier.buffers.size(), 1);
    EXPECT_EQ(barrier.buffers[0].srcAccessMask, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

    EXPECT_EQ(states.last(image, 0)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // no state in this: the first use of next becomes the first use
    EXPECT_EQ(states.first(other, 0)->layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(states.last(buffer)->scope.access, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
}

JVK_TEST(ResourceStates, append_without_a_first_use_of_next_needs_no_barrier) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    ResourceStates next{};
    cvk::Barrier barrier{};
    states.assume(image, UNDEFINED);
    next.assume(image, {.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .scope = {}});

    states.append(next, barrier);

    EXPECT_TRUE(barrier.empty());
    EXPECT_EQ(states.last(image, 0)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

JVK_TEST(ResourceStates, follows_the_handle_when_the_image_moves) {
    test::Context const context{};
    Image image{context.core, IMAGE_CONFIG, EXTENT};
    ResourceStates states{};
    states.assume(image, {.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .scope = {}});

    Image const moved{std::move(image)};

    EXPECT_EQ(states.last(moved, 0)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

JVK_TEST(ResourceStates, erase_and_clear) {
    test::Context const context{};
    Image const first{context.core, IMAGE_CONFIG, EXTENT};
    Image const second{context.core, IMAGE_CONFIG, EXTENT};
    BaseBuffer const buffer{context.core, BUFFER_CONFIG, SIZE};
    ResourceStates states{};
    states.assume(first, UNDEFINED);
    states.assume(second, UNDEFINED);
    states.assume(buffer, BufferState{});

    states.erase(first);
    EXPECT_FALSE(states.contains(first));
    EXPECT_TRUE(states.contains(second));

    states.erase(buffer);
    EXPECT_FALSE(states.contains(buffer));

    states.clear();
    EXPECT_TRUE(states.empty());
}
}
