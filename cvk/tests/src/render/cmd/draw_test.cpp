#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/data/buffer/buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/pipeline.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>


namespace cvk {
namespace {
    constexpr VkExtent2D EXTENT{16, 16};
    constexpr std::array COLOR_FORMATS{VK_FORMAT_R8G8B8A8_UNORM};
    constexpr std::array<float, 4> GREEN{0.f, 1.f, 0.f, 1.f};

    constexpr BaseBuffer::Config READBACK_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT},
    };

    struct Vec3 {
        float x, y, z;
    };

    /**
     * @brief one triangle that covers the whole viewport
     */
    constexpr std::array<Vec3, 3> FULLSCREEN{Vec3{-1.f, -1.f, 0.f}, Vec3{3.f, -1.f, 0.f}, Vec3{-1.f, 3.f, 0.f}};

    template<class T>
    constexpr Buffer<T>::Config host_written(VkBufferUsageFlags usage) {
        return {.usage = usage, .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT}};
    }

    /**
     * @brief pipeline that fills the triangle with the push constant color
     */
    struct Fill {
        explicit Fill(Core const& core) : layout{
                core,
                PipelineLayout::Config{.pushConstantRanges = {{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GREEN)}}},
                create
            },
            vertex{core, VK_SHADER_STAGE_VERTEX_BIT, test::shaders::FILL_VERT},
            fragment{core, VK_SHADER_STAGE_FRAGMENT_BIT, test::shaders::TRIANGLE_FRAG},
            pipeline{core, layout, config(), create},
            vertices{core, host_written<Vec3>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), FULLSCREEN.size()} {
            vertices.write(FULLSCREEN);
        }

        [[nodiscard]] Pipeline::Config config() const {
            Pipeline::Config config{COLOR_FORMATS};
            config.addStage(vertex).addStage(fragment);
            config.vertexBindings = {{.binding = 0, .stride = sizeof(Vec3), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}};
            config.vertexAttributes = {
                {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
            };
            config.rasterization.cullMode = VK_CULL_MODE_NONE;
            return config;
        }

        PipelineLayout layout;
        Shader vertex;
        Shader fragment;
        Pipeline pipeline;
        Buffer<Vec3> vertices;
    };

    /**
     * @brief clears an attachment to black, draws with draw() and expects every pixel green
     */
    template<class Draw>
    void expect_filled(test::Context const& context, Fill const& fill, Draw draw) {
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
        BaseBuffer const readback{context.core, READBACK_CONFIG, size_t{EXTENT.width} * EXTENT.height * 4};

        test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
            Barrier transition{};
            transition.addTransition(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
            transition.execute(cmd);
            cmd.beginRendering(
                {
                    .colorAttachments = {
                        RenderingAttachment{
                            .view = &view,
                            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                            .clearValue = {.color = {.float32 = {0.f, 0.f, 0.f, 1.f}}},
                        },
                    },
                }
            );

            cmd.bindPipeline(fill.pipeline);
            cmd.setViewport();
            cmd.setScissor();
            cmd.bindVertexBuffer(0, fill.vertices);
            cmd.pushConstants(fill.layout, VK_SHADER_STAGE_FRAGMENT_BIT, GREEN);
            draw(cmd);

            cmd.endRendering();

            Barrier barrier{};
            barrier.add(
                image,
                ImageBarrierInfo::LayoutTransition(
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
                    {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT}
                )
            );
            barrier.execute(cmd);

            test::copy_to_buffer(context, cmd, image, readback, 0, 0);
            test::host_read_barrier(context, cmd);
        });

        auto const pixels = readback.memory().map(readback.size());
        readback.memory().invalidate();
        constexpr std::array<unsigned char, 4> green{0, 255, 0, 255};
        for(auto const pixel : pixels | std::views::chunk(4))
            EXPECT_TRUE(std::ranges::equal(pixel, green, {}, [](char c) { return static_cast<unsigned char>(c); }));
        readback.memory().unmap();
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

CVK_TEST(Draw, draw_indexed) {
    test::Context const context{};
    Fill const fill{context.core};
    Buffer<uint16_t> indices{context.core, host_written<uint16_t>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT), 3};
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
        host_written<VkDrawIndirectCommand>(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
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
    Buffer<uint32_t> indices{context.core, host_written<uint32_t>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT), 3};
    indices.write(std::array<uint32_t, 3>{0, 1, 2});
    Buffer<VkDrawIndexedIndirectCommand> commands{
        context.core,
        host_written<VkDrawIndexedIndirectCommand>(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
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
