#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/data/buffer/buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/pipeline/graphics_pipeline.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>


namespace cvk {
namespace {
    using test::GREEN;
    using test::host_written;
    using test::Vec3;

    constexpr VkExtent2D EXTENT{16, 16};
    constexpr std::array COLOR_FORMATS{VK_FORMAT_R8G8B8A8_UNORM};

    /**
     * the clear color of `expect_filled()`
     */
    constexpr std::array<unsigned char, 4> BLACK_PIXEL{0, 0, 0, 255};

    /**
     * @return the pixels inside the extent from offset `0` are green, the others black
     */
    [[nodiscard]] bool filled_from_origin(BaseBuffer const& readback, VkExtent2D filled) {
        auto const pixels = readback.map();
        readback.invalidate();
        auto const asByte = [](std::byte b) { return std::to_integer<unsigned char>(b); };
        for(uint32_t y = 0; y < EXTENT.height; ++y)
            for(uint32_t x = 0; x < EXTENT.width; ++x) {
                auto const& expected = x < filled.width && y < filled.height ? test::GREEN_PIXEL : BLACK_PIXEL;
                auto const pixel = pixels.span().subspan((size_t{y} * EXTENT.width + x) * 4, 4);
                if(!std::ranges::equal(pixel, expected, {}, asByte)) return false;
            }
        return true;
    }

    /**
     * pipeline that fills the triangle with the push constant color
     */
    struct Fill {
        explicit Fill(Core const& core) : layout{
                core,
                PipelineLayout::Config{.pushConstantRanges = {{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GREEN)}}},
                create
            },
            vertex{core, VK_SHADER_STAGE_VERTEX_BIT, test::shaders::FILL_VERT},
            fragment{core, VK_SHADER_STAGE_FRAGMENT_BIT, test::shaders::TRIANGLE_FRAG},
            pipeline{layout, test::fullscreen_config(vertex, fragment, COLOR_FORMATS), create},
            vertices{core, host_written(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), test::FULLSCREEN.size()} {
            vertices.write(test::FULLSCREEN);
        }

        PipelineLayout layout;
        Shader vertex;
        Shader fragment;
        GraphicsPipeline pipeline;
        Buffer<Vec3> vertices;
    };

    /**
     * clears an attachment to black, draws with `draw()` and expects the pixels in the extent green
     * @param filled from offset `0`, the other pixels stay black
     */
    template<class Draw>
    void expect_filled(test::Context const& context, Fill const& fill, Draw draw, VkExtent2D filled = EXTENT) {
        Image const image{
            context.core,
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .format = COLOR_FORMATS[0],
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            },
            EXTENT
        };
        ImageView const view{image, {}, create};
        BaseBuffer const readback{context.core, test::READBACK_CONFIG, size_t{EXTENT.width} * EXTENT.height * 4};

        test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
            Barrier transition{};
            transition.addTransition(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
            transition.execute(cmd);
            auto const rendering = cmd.beginRendering(
                {
                    .colorAttachments = {
                        RenderingAttachment{
                            .view = view,
                            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                            .clearValue = {.color = {.float32 = {0.f, 0.f, 0.f, 1.f}}},
                        },
                    },
                }
            );

            cmd.bindPipeline(fill.pipeline);
            cmd.bindVertexBuffer(0, fill.vertices);
            // a span pushes its elements, not itself
            cmd.pushConstants(fill.layout, VK_SHADER_STAGE_FRAGMENT_BIT, std::span<float const>{GREEN});
            draw(cmd);

            cmd.endRendering();

            Barrier barrier{};
            barrier.addTransition(
                image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            barrier.execute(cmd);

            test::copy_to_buffer(context, cmd, image, readback, 0, 0);
            test::host_read_barrier(context, cmd);
        });

        EXPECT_TRUE(filled_from_origin(readback, filled));
    }
}

CVK_TEST(Draw, index_type_follows_the_element) {
    static_assert(index_type<uint16_t> == VK_INDEX_TYPE_UINT16);
    static_assert(index_type<uint32_t> == VK_INDEX_TYPE_UINT32);
    static_assert(!index_element<uint8_t>);
    static_assert(!index_element<int32_t>);
}

CVK_TEST(Draw, draw) {
    test::Context const context{};
    Fill const fill{context.core};

    expect_filled(context, fill, [](PrimaryCmdBuffer const& cmd) { cmd.draw(3); });
}

CVK_TEST(Draw, viewport_of_an_extent_starts_at_offset_0) {
    static constexpr VkExtent2D LEFT_HALF{EXTENT.width / 2, EXTENT.height};
    test::Context const context{};
    Fill const fill{context.core};

    expect_filled(
        context,
        fill,
        [](PrimaryCmdBuffer const& cmd) {
            cmd.setViewport(LEFT_HALF);
            cmd.setScissor(EXTENT);
            cmd.draw(3);
        },
        LEFT_HALF
    );
}

CVK_TEST(Draw, scissor_of_an_extent_starts_at_offset_0) {
    static constexpr VkExtent2D TOP_HALF{EXTENT.width, EXTENT.height / 2};
    test::Context const context{};
    Fill const fill{context.core};

    expect_filled(
        context,
        fill,
        [](PrimaryCmdBuffer const& cmd) {
            cmd.setViewport(EXTENT);
            cmd.setScissor(TOP_HALF);
            cmd.draw(3);
        },
        TOP_HALF
    );
}

CVK_TEST(Draw, draw_indexed) {
    test::Context const context{};
    Fill const fill{context.core};
    Buffer<uint16_t> indices{context.core, host_written(VK_BUFFER_USAGE_INDEX_BUFFER_BIT), 3};
    indices.write(std::array<uint16_t, 3>{0, 1, 2});

    expect_filled(context, fill, [&](PrimaryCmdBuffer const& cmd) {
        cmd.bindIndexBuffer(indices);
        cmd.drawIndexed(3);
    });
}

CVK_TEST(Draw, draw_indirect) {
    test::Context const context{};
    Fill const fill{context.core};
    Buffer<VkDrawIndirectCommand> commands{
        context.core,
        host_written(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        1
    };
    commands.write(
        std::array{VkDrawIndirectCommand{.vertexCount = 3, .instanceCount = 1, .firstVertex = 0, .firstInstance = 0}}
    );

    expect_filled(context, fill, [&](PrimaryCmdBuffer const& cmd) { cmd.drawIndirect(commands); });
}

CVK_TEST(Draw, draw_indexed_indirect) {
    test::Context const context{};
    Fill const fill{context.core};
    Buffer<uint32_t> indices{context.core, host_written(VK_BUFFER_USAGE_INDEX_BUFFER_BIT), 3};
    indices.write(std::array<uint32_t, 3>{0, 1, 2});
    Buffer<VkDrawIndexedIndirectCommand> commands{
        context.core,
        host_written(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        1
    };
    commands.write(
        std::array{
            VkDrawIndexedIndirectCommand{
                .indexCount = 3,
                .instanceCount = 1,
                .firstIndex = 0,
                .vertexOffset = 0,
                .firstInstance = 0,
            },
        }
    );

    expect_filled(context, fill, [&](PrimaryCmdBuffer const& cmd) {
        cmd.bindIndexBuffer(indices);
        cmd.drawIndexedIndirect(commands);
    });
}
}
