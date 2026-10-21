#include "context.hpp"
#include "test.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <array>


namespace cvk {
namespace {
    constexpr VkExtent2D EXTENT{16, 16};

    constexpr Image::Config IMAGE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .mipLevels = mip_level_count(EXTENT),
    };

    constexpr BarrierScope TRANSFER_WRITE{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    constexpr BarrierScope FRAGMENT_READ{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};

    constexpr VkImageAspectFlags COLOR = VK_IMAGE_ASPECT_COLOR_BIT;
    constexpr VkImageAspectFlags DEPTH = VK_IMAGE_ASPECT_DEPTH_BIT;
    constexpr VkPipelineStageFlags2 TESTS = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
        | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
}

CVK_TEST(Barrier, layout_transition_from_undefined_waits_for_the_dst_stages) {
    constexpr auto toTransfer = layout_transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED, COLOR);
    static_assert(toTransfer.has_value());
    static_assert(toTransfer->src.stages == VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    static_assert(toTransfer->src.access == VK_ACCESS_2_NONE);
    static_assert(toTransfer->dst.access == VK_ACCESS_2_TRANSFER_WRITE_BIT);

    constexpr auto toLocalRead = layout_transition(
        VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
        VK_IMAGE_LAYOUT_UNDEFINED,
        COLOR
    );
    static_assert(toLocalRead.has_value());
    static_assert((toLocalRead->dst.access & VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT) != 0);

    constexpr auto toUndefined = layout_transition(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        COLOR
    );
    static_assert(!toUndefined.has_value());
}

CVK_TEST(Barrier, layout_transition_src_from_the_old_dst_from_the_new_layout) {
    constexpr auto readback = layout_transition(
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        COLOR
    );
    static_assert(readback.has_value());
    static_assert(readback->src.stages == VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    static_assert(readback->src.access == VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    static_assert(readback->dst.stages == VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    static_assert(readback->dst.access == VK_ACCESS_2_TRANSFER_READ_BIT);
}

CVK_TEST(Barrier, layout_transition_src_keeps_only_the_writes) {
    constexpr auto renderAgain = layout_transition(
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        COLOR
    );
    static_assert(renderAgain.has_value());
    static_assert(renderAgain->src.stages == VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    static_assert(renderAgain->src.access == VK_ACCESS_2_NONE);
}

CVK_TEST(Barrier, layout_transition_aspects_pick_color_or_depth) {
    constexpr auto color = layout_transition(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED, COLOR);
    constexpr auto depth = layout_transition(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED, DEPTH);
    static_assert(color->dst.stages == VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    static_assert(depth->dst.stages == TESTS);

    constexpr auto depthLocalRead = layout_scope(VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR, DEPTH);
    static_assert(depthLocalRead->stages == (TESTS | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT));
}

CVK_TEST(Barrier, layout_transition_shader_stages_replace_the_fragment_shader) {
    constexpr auto toCompute = layout_transition(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        COLOR,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    );
    static_assert(toCompute->dst.stages == VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    constexpr auto fromCompute = layout_transition(
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        COLOR,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    );
    static_assert(fromCompute->src.stages == VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    // input attachments are fragment shader only
    constexpr auto localRead = layout_scope(
        VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
        COLOR,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
    );
    static_assert((localRead->stages & VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT) == 0);
}

CVK_TEST(Barrier, every_pair_of_known_layouts_has_a_transition) {
    static_assert([] {
        constexpr std::array layouts{
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };
        for(auto const aspects : {COLOR, DEPTH})
            for(auto const newLayout : layouts)
                for(auto const oldLayout : layouts)
                    if(!layout_transition(newLayout, oldLayout, aspects).has_value()) return false;
        return true;
    }());
}

CVK_TEST(Barrier, layout_transition_info_covers_the_remaining_mip_levels) {
    constexpr auto info = ImageBarrierInfo::LayoutTransition(
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_UNDEFINED,
        TRANSFER_WRITE,
        BarrierScope{}
    );

    static_assert(info.firstMipLevel == 0);
    static_assert(info.mipLevels == VK_REMAINING_MIP_LEVELS);
    static_assert(info.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    static_assert(info.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED);
    static_assert(info.dst.access == VK_ACCESS_2_TRANSFER_WRITE_BIT);
    static_assert(info.src.access == VK_ACCESS_2_NONE);
}

CVK_TEST(Barrier, queue_transition_info_keeps_the_layout) {
    constexpr auto info = ImageBarrierInfo::QueueTransition(
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        {.queueFamilyIndex = 1},
        {.queueFamilyIndex = 0}
    );

    static_assert(info.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    static_assert(info.oldLayout == info.newLayout);
    static_assert(info.dst.queueFamilyIndex == 1);
    static_assert(info.src.queueFamilyIndex == 0);
}

CVK_TEST(Barrier, memory_barriers_are_constexpr) {
    static_assert([] {
        Barrier barrier{VK_DEPENDENCY_BY_REGION_BIT};
        barrier.add(FRAGMENT_READ, TRANSFER_WRITE);
        bool const added = !barrier.empty();
        bool const ordered = barrier.memory.front().dst.access == FRAGMENT_READ.access;
        barrier.clear();
        return added && ordered && barrier.empty();
    }());
}

CVK_TEST(Barrier, aggregate_is_constexpr) {
    static_assert([] {
        //TEMP not const: gcc 16 rejects a const aggregate with a vector and another member in a constant expression
        Barrier barrier{
            .flags = VK_DEPENDENCY_BY_REGION_BIT,
            .memory = {{TRANSFER_WRITE, FRAGMENT_READ}},
        };
        return !barrier.empty() && barrier.memory.size() == 1 && barrier.buffers.empty();
    }());
}

CVK_TEST(Barrier, add_and_clear) {
    test::Context const context{};
    BaseBuffer const buffer{context.core, {.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT}, 64};
    Barrier barrier{};
    EXPECT_TRUE(barrier.empty());

    barrier.add(FRAGMENT_READ, TRANSFER_WRITE);
    barrier.add(buffer, {.src = TRANSFER_WRITE, .dst = FRAGMENT_READ});
    EXPECT_FALSE(barrier.empty());

    barrier.clear();
    EXPECT_TRUE(barrier.empty());
}

CVK_TEST(Barrier, execute_memory_and_buffer_barriers) {
    test::Context const context{};
    BaseBuffer const buffer{context.core, {.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT}, 64};
    Barrier barrier{VK_DEPENDENCY_BY_REGION_BIT};
    barrier.add(FRAGMENT_READ, TRANSFER_WRITE);
    barrier.add(buffer, {.src = TRANSFER_WRITE, .dst = FRAGMENT_READ, .offset = 16, .size = 32});

    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) { barrier.execute(primary); });

    EXPECT_EQ(barrier.flags, VK_DEPENDENCY_BY_REGION_BIT);
}

CVK_TEST(Barrier, image_layout_transition) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};

    Barrier const barrier{
        .images = {
            {
                &image,
                ImageBarrierInfo::LayoutTransition(
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    TRANSFER_WRITE,
                    {}
                ),
            },
        },
    };
    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) { barrier.execute(primary); });
}

CVK_TEST(Barrier, add_transition_on_a_mip_range) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};

    // the validation layers check every old layout
    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) {
        Barrier toTransfer{};
        toTransfer.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toTransfer.execute(primary);

        Barrier toRead{};
        toRead.addTransition(
            image,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            Barrier::INFERRED_SHADER_STAGES,
            1,
            2
        );
        toRead.addTransition(
            image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            Barrier::INFERRED_SHADER_STAGES,
            3
        );
        toRead.execute(primary);

        Barrier back{};
        back.addTransition(
            image,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            Barrier::INFERRED_SHADER_STAGES,
            3
        );
        back.execute(primary);
    });
}

CVK_TEST(Barrier, memory_only_image_barrier) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};

    Barrier barrier{};
    barrier.add(image, {.src = TRANSFER_WRITE, .dst = TRANSFER_WRITE});
    test::record_and_submit(context, [&](PrimaryCmdBuffer const& primary) { barrier.execute(primary); });
}
}
