#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/data/buffer/buffer.hpp"
#include "cvk/data/descriptor/descriptor_pool.hpp"
#include "cvk/data/descriptor/descriptor_set.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/pipeline/graphics_pipeline.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <array>
#include <cstddef>
#include <utility>


namespace cvk {
namespace {
    using test::GREEN;
    using test::Vec3;

    constexpr VkExtent2D EXTENT{16, 16};
    constexpr std::array COLOR_FORMATS{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};

    constexpr BarrierScope COLOR_WRITE{
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    };
    constexpr BarrierScope INPUT_READ{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT};

    /**
     * @brief location 0 -> attachment 0, attachment 1 without output
     */
    AttachmentMapping write_first() { return {.locations = {0, VK_ATTACHMENT_UNUSED}}; }

    /**
     * @brief attachment 0 -> input attachment 0, location 0 -> attachment 1
     */
    AttachmentMapping copy_first_to_second() {
        return {.locations = {VK_ATTACHMENT_UNUSED, 0}, .colorInputIndices = {0, VK_ATTACHMENT_UNUSED}};
    }

    /**
     * @brief attachment 0: input attachment, attachment 1: transfer src, both with a view
     */
    struct Attachments {
        explicit Attachments(Core const& core) : first{
                core,
                {
                    .format = COLOR_FORMATS[0],
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                },
                EXTENT
            },
            second{
                core,
                {
                    .format = COLOR_FORMATS[1],
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                },
                EXTENT
            },
            firstView{first, {}, create},
            secondView{second, {}, create} {}

        /**
         * @brief records the transitions: first -> rendering local read, second -> color attachment
         */
        void toRendering(BaseCmdBuffer const& cmd) const {
            Barrier barrier{};
            barrier.addTransition(first, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR, VK_IMAGE_LAYOUT_UNDEFINED);
            barrier.addTransition(second, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
            barrier.execute(cmd);
        }

        /**
         * @brief both attachments cleared to black
         */
        [[nodiscard]] RenderingInfo renderingInfo(VkRenderingFlags flags = 0, AttachmentMapping mapping = {}) const {
            auto const clear = [](ImageView const& view, VkImageLayout layout) {
                return RenderingAttachment{
                    .view = &view,
                    .layout = layout,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.color = {.float32 = {0.f, 0.f, 0.f, 1.f}}},
                };
            };
            return RenderingInfo{
                .colorAttachments = {
                    clear(firstView, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR),
                    clear(secondView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
                },
                .flags = flags,
                .attachmentMapping = std::move(mapping),
            };
        }

        Image first;
        Image second;
        ImageView firstView;
        ImageView secondView;
    };

    /**
     * @brief fullscreen pipeline for the attachments
     */
    GraphicsPipeline::Config fullscreen(Shader const& vertex, Shader const& fragment, AttachmentMapping mapping) {
        auto config = test::fullscreen_config(vertex, fragment, COLOR_FORMATS);
        config.attachmentMapping = std::move(mapping);
        return config;
    }

    /**
     * @brief binding 0: input attachment
     */
    DescriptorSetLayout::Config input_attachment() {
        DescriptorSetLayout::Config config{};
        config.add(0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        return config;
    }

    /**
     * @brief write: push constant color -> attachment 0, read: attachment 0 -> attachment 1
     */
    struct Draws {
        Draws(Core const& core, ImageView const& first) :
            vertex{core, VK_SHADER_STAGE_VERTEX_BIT, test::shaders::FILL_VERT},
            color{core, VK_SHADER_STAGE_FRAGMENT_BIT, test::shaders::TRIANGLE_FRAG},
            localRead{core, VK_SHADER_STAGE_FRAGMENT_BIT, test::shaders::LOCAL_READ_FRAG},
            writeLayout{core, {.pushConstantRanges = {{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GREEN)}}}, create},
            write{writeLayout, fullscreen(vertex, color, write_first()), create},
            inputLayout{core, input_attachment(), create},
            readLayout{core, {.setLayouts = {&inputLayout}}, create},
            read{readLayout, fullscreen(vertex, localRead, copy_first_to_second()), create},
            pool{core, DescriptorPool::Config::ForLayouts({{&inputLayout}}), create},
            input{pool, inputLayout, create},
            vertices{core, test::host_written(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), test::FULLSCREEN.size()} {
            // input attachment: VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR from the binding type
            input.write({0, first});
            vertices.write(test::FULLSCREEN);
        }

        /**
         * @brief records the draw of @ref write
         * @pre the command buffer has a viewport and a scissor
         */
        void drawWrite(BaseCmdBuffer& cmd) const {
            cmd.bindVertexBuffer(0, vertices);
            cmd.bindPipeline(write);
            cmd.pushConstants(writeLayout, VK_SHADER_STAGE_FRAGMENT_BIT, GREEN);
            cmd.draw(3);
        }

        Shader vertex;
        Shader color;
        Shader localRead;
        PipelineLayout writeLayout;
        GraphicsPipeline write;
        DescriptorSetLayout inputLayout;
        PipelineLayout readLayout;
        GraphicsPipeline read;
        DescriptorPool pool;
        DescriptorSet input;
        Buffer<Vec3> vertices;
    };
}

CVK_TEST(AttachmentMapping, empty_maps_to_the_default) {
    static_assert([] {
        AttachmentMapping const mapping{};
        auto const locations = mapping.locationInfo(2);
        auto const indices = mapping.inputIndexInfo(2);

        return locations.colorAttachmentCount == 2 && locations.pColorAttachmentLocations == nullptr
            && indices.colorAttachmentCount == 2 && indices.pColorAttachmentInputIndices == nullptr
            && indices.pDepthInputAttachmentIndex == nullptr && indices.pStencilInputAttachmentIndex == nullptr;
    }());
}

CVK_TEST(AttachmentMapping, infos_point_into_the_mapping) {
    static_assert([] {
        //TEMP not const: gcc 16 rejects a const aggregate with a vector and another member in a constant expression
        AttachmentMapping mapping{
            .locations = {1, 0},
            .colorInputIndices = {VK_ATTACHMENT_UNUSED, 0},
            .depthInputIndex = 1,
        };
        auto const locations = mapping.locationInfo(2);
        auto const indices = mapping.inputIndexInfo(2);

        return locations.pColorAttachmentLocations[0] == 1 && locations.pColorAttachmentLocations[1] == 0
            && indices.pColorAttachmentInputIndices[1] == 0 && *indices.pDepthInputAttachmentIndex == 1
            && indices.pStencilInputAttachmentIndex == nullptr;
    }());
}

CVK_TEST(AttachmentMapping, empty_equals_the_default) {
    static_assert([] {
        //TEMP not const: gcc 16 rejects a const aggregate with a vector and another member in a constant expression
        AttachmentMapping identity{.locations = {0, 1}, .colorInputIndices = {0, 1}};
        AttachmentMapping swapped{.locations = {1, 0}};
        AttachmentMapping depth{.depthInputIndex = 0};

        return AttachmentMapping{} == identity && AttachmentMapping{} != swapped && AttachmentMapping{} != depth;
    }());
}

CVK_TEST(LocalRead, draw_reads_an_attachment_of_the_same_rendering) {
    test::Context const context{};
    Attachments const attachments{context.core};
    Draws const draws{context.core, attachments.firstView};
    BaseBuffer const readback{context.core, test::READBACK_CONFIG, size_t{EXTENT.width} * EXTENT.height * 4};

    test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
        attachments.toRendering(cmd);

        // the begin sets the mapping of the first draw, the bind of the second pipeline its own
        cmd.beginRendering(attachments.renderingInfo(0, write_first()));
        EXPECT_EQ(cmd.rendering()->attachmentMapping, write_first());

        draws.drawWrite(cmd);

        Barrier toInput{VK_DEPENDENCY_BY_REGION_BIT};
        toInput.add(
            attachments.first,
            {
                .src = COLOR_WRITE,
                .dst = INPUT_READ,
                .newLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
                .oldLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            }
        );
        toInput.execute(cmd);

        cmd.bindPipeline(draws.read);
        EXPECT_EQ(cmd.rendering()->attachmentMapping, copy_first_to_second());
        cmd.bindDescriptorSet(draws.input, draws.readLayout, 0);
        cmd.draw(3);
        cmd.endRendering();

        Barrier toTransfer{};
        toTransfer.add(
            attachments.second,
            ImageBarrierInfo::LayoutTransition(
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT},
                COLOR_WRITE
            )
        );
        toTransfer.execute(cmd);

        test::copy_to_buffer(context, cmd, attachments.second, readback, 0, 0);
        test::host_read_barrier(context, cmd);
    });

    EXPECT_TRUE(test::every_pixel_is(readback, test::GREEN_PIXEL));
}

CVK_TEST(LocalRead, secondary_inherits_the_mapping) {
    test::Context const context{};
    Attachments const attachments{context.core};
    Draws const draws{context.core, attachments.firstView};
    CmdPool const pool{context.core, test::pool_config(context.core), create};
    SecondaryCmdBuffer secondary{pool, create};

    test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
        attachments.toRendering(cmd);
        cmd.beginRendering(attachments.renderingInfo(VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT));
        EXPECT_EQ(cmd.rendering()->attachmentMapping, AttachmentMapping{});

        cmd.setAttachmentMapping(write_first());
        EXPECT_EQ(cmd.rendering()->attachmentMapping, write_first());

        // the draw needs the mapping of the write pipeline in the secondary
        auto const inheritance = cmd.inheritance();
        EXPECT_EQ(inheritance.attachmentMapping, write_first());
        secondary.begin(inheritance, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        secondary.setViewport(
            {0.f, 0.f, static_cast<float>(EXTENT.width), static_cast<float>(EXTENT.height), 0.f, 1.f}
        );
        secondary.setScissor({{0, 0}, EXTENT});
        draws.drawWrite(secondary);
        secondary.end();

        cmd.execute(secondary);
        cmd.endRendering();
    });
}
}
