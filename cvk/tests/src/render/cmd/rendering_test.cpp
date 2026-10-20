#include "context.hpp"
#include "test.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>


namespace cvk {
namespace {
    constexpr VkExtent2D EXTENT{16, 16};

    constexpr Image::Config COLOR_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    };

    constexpr Image::Config LOCAL_READ_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
    };

    constexpr BarrierScope COLOR_WRITE{
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    };

    RenderingAttachment clear_red(ImageView const& view, VkImageLayout layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) {
        return RenderingAttachment{
            .view = &view,
            .layout = layout,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.color = {.float32 = {1.f, 0.f, 0.f, 1.f}}},
        };
    }
}

CVK_TEST(Rendering, clear_a_color_attachment) {
    test::Context const context{};
    Image const image{context.core, COLOR_CONFIG, EXTENT};
    ImageView const view{image, {}, create};
    BaseBuffer const readback{
        context.core,
        {.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT}},
        size_t{EXTENT.width} * EXTENT.height * 4
    };

    test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
        Barrier transition{};
        transition.addTransition(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        transition.execute(cmd);

        cmd.beginRendering({.colorAttachments = {clear_red(view)}});
        cmd.endRendering();

        Barrier barrier{};
        barrier.add(
            image,
            ImageBarrierInfo::LayoutTransition(
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT},
                COLOR_WRITE
            )
        );
        barrier.execute(cmd);

        test::copy_to_buffer(context, cmd, image, readback, 0, 0);
        test::host_read_barrier(context, cmd);
    });

    auto const pixels = readback.map();
    readback.invalidate();
    auto const red = std::array<unsigned char, 4>{255, 0, 0, 255};
    auto const asByte = [](std::byte b) { return std::to_integer<unsigned char>(b); };
    for(auto const pixel : pixels | std::views::chunk(4))
        EXPECT_TRUE(std::ranges::equal(pixel, red, {}, asByte));
}

CVK_TEST(Rendering, tracks_the_current_rendering) {
    test::Context const context{};
    Image const image{context.core, COLOR_CONFIG, EXTENT};
    ImageView const view{image, {}, create};

    test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
        Barrier transition{};
        transition.addTransition(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        transition.execute(cmd);
        EXPECT_EQ(cmd.rendering(), nullptr);

        cmd.beginRendering({.colorAttachments = {clear_red(view)}});
        auto const* rendering = cmd.rendering();
        ASSERT_NE(rendering, nullptr);
        ASSERT_EQ(rendering->colorAttachments.size(), 1);
        EXPECT_EQ(rendering->colorAttachments[0].view.get(), &view);
        ASSERT_TRUE(rendering->area.has_value());
        EXPECT_EQ(rendering->area->extent.width, EXTENT.width);
        EXPECT_EQ(rendering->area->extent.height, EXTENT.height);

        auto const inheritance = cmd.inheritance();
        ASSERT_EQ(inheritance.colorFormats.size(), 1);
        EXPECT_EQ(inheritance.colorFormats[0], COLOR_CONFIG.format);

        cmd.endRendering();
        EXPECT_EQ(cmd.rendering(), nullptr);
    });
}

CVK_TEST(Rendering, clear_a_depth_attachment) {
    test::Context const context{};
    Image const image{context.core, Image::Config::DepthBuffer(VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT), EXTENT};
    ImageView const view{image, {}, create};

    test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
        Barrier transition{};
        transition.addTransition(image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        transition.execute(cmd);

        cmd.beginRendering(
            {
                .depthAttachment = RenderingAttachment{
                    .view = &view,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .clearValue = {.depthStencil = {1.f, 0}},
                },
            }
        );
        EXPECT_EQ(cmd.inheritance().depthFormat, VK_FORMAT_D16_UNORM);
        cmd.endRendering();
    });
}

CVK_TEST(Rendering, secondary_inherits_the_rendering) {
    test::Context const context{};
    Image const image{context.core, COLOR_CONFIG, EXTENT};
    ImageView const view{image, {}, create};
    CmdPool const pool{context.core, CmdPool::Config{context.core.queue(0)}, create};
    std::array<SecondaryCmdBuffer, 1> secondaries{SecondaryCmdBuffer{pool, create}};

    test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
        Barrier transition{};
        transition.addTransition(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        transition.execute(cmd);
        cmd.beginRendering(
            {.colorAttachments = {clear_red(view)}, .flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT}
        );

        auto& secondary = secondaries[0];
        secondary.begin(cmd.inheritance(), VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        EXPECT_TRUE(secondary.recording());
        EXPECT_EQ(secondary.rendering(), nullptr);
        secondary.end();

        cmd.execute(secondaries);
        cmd.endRendering();
    });
}

CVK_TEST(Rendering, local_read_inside_the_rendering) {
    test::Context const context{};
    Image const image{context.core, LOCAL_READ_CONFIG, EXTENT};
    ImageView const view{image, {}, create};

    test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
        Barrier transition{};
        transition.addTransition(image, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR, VK_IMAGE_LAYOUT_UNDEFINED);
        transition.execute(cmd);

        cmd.beginRendering({.colorAttachments = {clear_red(view, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR)}});
        cmd.setAttachmentMapping({.locations = {0}, .colorInputIndices = {0}});

        Barrier barrier{VK_DEPENDENCY_BY_REGION_BIT};
        barrier.add(
            image,
            {
                .src = COLOR_WRITE,
                .dst = {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT},
                .newLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
                .oldLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            }
        );
        barrier.execute(cmd);

        cmd.endRendering();
    });
}
}
