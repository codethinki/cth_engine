#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/data/buffer/buffer.hpp"
#include "cvk/data/descriptor/descriptor_pool.hpp"
#include "cvk/data/descriptor/descriptor_set.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/data/img/sampler.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/pipeline/graphics_pipeline.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>


namespace cvk {
namespace {
    using test::host_written;
    using test::Vec3;

    constexpr VkExtent2D EXTENT{16, 16};
    constexpr std::array COLOR_FORMATS{VK_FORMAT_R8G8B8A8_UNORM};

    /**
     * @brief fullscreen pipeline with one descriptor set and the fragment shader
     */
    struct Fill {
        Fill(Core const& core, DescriptorSetLayout const& setLayout, std::span<uint32_t const> fragmentCode,
            std::vector<VkPushConstantRange> pushConstants = {}) : layout{
                core,
                PipelineLayout::Config{.setLayouts = {&setLayout}, .pushConstantRanges = std::move(pushConstants)},
                create
            },
            vertex{core, VK_SHADER_STAGE_VERTEX_BIT, test::shaders::FILL_VERT},
            fragment{core, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentCode},
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
     * @brief target image, its view and a readback buffer
     */
    struct Target {
        explicit Target(Core const& core) : image{
                core,
                {
                    .format = COLOR_FORMATS[0],
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                },
                EXTENT
            },
            view{image, {}, create},
            readback{core, test::READBACK_CONFIG, size_t{EXTENT.width} * EXTENT.height * 4} {}

        Image image;
        ImageView view;
        BaseBuffer readback;
    };

    /**
     * @brief records: clear the target, draw with draw(), copy the target into the readback buffer
     */
    void record_fill(
        test::Context const& context,
        BaseCmdBuffer& cmd,
        Fill const& fill,
        Target const& target,
        std::function<void(BaseCmdBuffer&)> const& draw
    ) {
        Barrier toAttachment{};
        toAttachment.addTransition(target.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toAttachment.execute(cmd);

        cmd.beginRendering(
            {
                .colorAttachments = {
                    RenderingAttachment{
                        .view = &target.view,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .clearValue = {.color = {.float32 = {0.f, 0.f, 0.f, 1.f}}},
                    },
                },
            }
        );
        cmd.bindPipeline(fill.pipeline);
        cmd.bindVertexBuffer(0, fill.vertices);
        draw(cmd);
        cmd.draw(3);
        cmd.endRendering();

        Barrier toTransfer{};
        toTransfer.add(
            target.image,
            ImageBarrierInfo::LayoutTransition(
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT},
                {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT}
            )
        );
        toTransfer.execute(cmd);

        test::copy_to_buffer(context, cmd, target.image, target.readback, 0, 0);
        test::host_read_barrier(context, cmd);
    }
}

CVK_TEST(DescriptorDraw, uniform_buffer_color) {
    test::Context const context{};
    DescriptorSetLayout::Config setConfig{};
    setConfig.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    DescriptorSetLayout const setLayout{context.core, setConfig, create};
    Fill const fill{context.core, setLayout, test::shaders::UNIFORM_COLOR_FRAG};
    Target const target{context.core};

    Buffer<std::array<float, 4>> color{context.core, host_written(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), 1};
    color.write(std::array<std::array<float, 4>, 1>{test::GREEN});

    DescriptorPool const pool{context.core, DescriptorPool::Config::ForLayouts({{&setLayout}}), create};
    DescriptorSet const set{pool, setLayout, create};
    set.write({0, color});

    test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
        record_fill(context, cmd, fill, target, [&](BaseCmdBuffer& c) { c.bindDescriptorSet(set, fill.layout, 0); });
    });

    EXPECT_TRUE(test::every_pixel_is(target.readback, test::GREEN_PIXEL));
}

namespace {
    /**
     * @brief draws a green texture
     * @param layout of the texture at the draw (VK_IMAGE_LAYOUT_MAX_ENUM => the default, shader read only)
     * @param afterBind true -> bindless binding of 64, the descriptor at element 17 comes after the recording,
     *  false -> one sampler2D, the descriptor comes before the recording, the layers check its layout at the draw
     */
    void draw_texture(test::Context const& context, VkImageLayout layout, bool afterBind) {
        uint32_t const index = afterBind ? 17 : 0;
        auto const textureLayout = layout != VK_IMAGE_LAYOUT_MAX_ENUM ? layout
            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        DescriptorSetLayout::Config setConfig{};
        setConfig.add(
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            afterBind ? 64 : 1,
            afterBind ? test::BINDLESS : 0
        );
        DescriptorSetLayout const setLayout{context.core, setConfig, create};
        Fill const fill{
            context.core,
            setLayout,
            afterBind ? std::span<uint32_t const>{test::shaders::BINDLESS_FRAG}
                      : std::span<uint32_t const>{test::shaders::TEXTURE_FRAG},
            {{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t)}}
        };
        Target const target{context.core};

        // 1x1 green texture
        Image const texture{
            context.core,
            {.format = VK_FORMAT_R8G8B8A8_UNORM, .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT},
            VkExtent2D{1, 1}
        };
        ImageView const view{texture, {}, create};
        Sampler const sampler{context.core, {}, create};
        Buffer<unsigned char> staging{context.core, test::STAGING_CONFIG, 4};
        staging.write(test::GREEN_PIXEL);

        uint32_t const variableCount = afterBind ? 32 : 0;
        DescriptorPool const pool{
            context.core,
            DescriptorPool::Config::ForLayouts({{&setLayout, 1, variableCount}}),
            create
        };
        DescriptorSet const set{pool, setLayout, variableCount, create};
        if(!afterBind) set.write({0, view, sampler, index, layout});

        CmdPool const cmdPool{context.core, test::pool_config(context.core), create};
        PrimaryCmdBuffer cmd{cmdPool, create};
        cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Barrier toTransfer{};
        toTransfer.addTransition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toTransfer.execute(cmd);
        texture.copy(cmd, staging);
        Barrier toShader{};
        toShader.add(
            texture,
            ImageBarrierInfo::LayoutTransition(
                textureLayout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT},
                {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT}
            )
        );
        toShader.execute(cmd);

        record_fill(context, cmd, fill, target, [&](BaseCmdBuffer& c) {
            c.bindDescriptorSet(set, fill.layout, 0);
            c.pushConstants(fill.layout, VK_SHADER_STAGE_FRAGMENT_BIT, index);
        });
        cmd.end();

        // update after bind: the bind is recorded, the descriptor comes now
        if(afterBind) set.write({0, view, sampler, index, layout});
        test::submit_and_wait(context, cmd);

        EXPECT_TRUE(test::every_pixel_is(target.readback, test::GREEN_PIXEL));
    }
}

CVK_TEST(DescriptorDraw, bindless_texture_written_after_bind) {
    test::Context const context{};
    draw_texture(context, VK_IMAGE_LAYOUT_MAX_ENUM, true);
}

CVK_TEST(DescriptorDraw, texture_in_the_layout_of_the_write) {
    test::Context const context{};
    draw_texture(context, VK_IMAGE_LAYOUT_GENERAL, false);
}
}
