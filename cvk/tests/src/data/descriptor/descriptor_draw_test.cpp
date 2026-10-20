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
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/pipeline.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <ranges>
#include <span>


namespace cvk {
namespace {
    constexpr VkExtent2D EXTENT{16, 16};
    constexpr std::array COLOR_FORMATS{VK_FORMAT_R8G8B8A8_UNORM};
    constexpr std::array<float, 4> GREEN{0.f, 1.f, 0.f, 1.f};
    constexpr std::array<unsigned char, 4> GREEN_PIXEL{0, 255, 0, 255};

    struct Vec3 {
        float x, y, z;
    };

    /**
     * @brief one triangle that covers the whole viewport
     */
    constexpr std::array<Vec3, 3> FULLSCREEN{Vec3{-1.f, -1.f, 0.f}, Vec3{3.f, -1.f, 0.f}, Vec3{-1.f, 3.f, 0.f}};

    template<class T>
    typename Buffer<T>::Config host_written(VkBufferUsageFlags usage) {
        return {.usage = usage, .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT}};
    }

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
            readback{
                core,
                {
                    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT},
                },
                size_t{EXTENT.width} * EXTENT.height * 4
            } {}

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
                {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
                {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT}
            )
        );
        toTransfer.execute(cmd);

        test::copy_to_buffer(context, cmd, target.image, target.readback, 0, 0);
        test::host_read_barrier(context, cmd);
    }

    void expect_green(Target const& target) {
        auto const pixels = target.readback.memory().map(target.readback.size());
        target.readback.memory().invalidate();
        auto const asByte = [](char c) { return static_cast<unsigned char>(c); };
        for(auto const pixel : pixels | std::views::chunk(4))
            EXPECT_TRUE(std::ranges::equal(pixel, GREEN_PIXEL, {}, asByte));
        target.readback.memory().unmap();
    }
}

CVK_TEST(DescriptorDraw, uniform_buffer_color) {
    test::Context const context{};
    DescriptorSetLayout::Config setConfig{};
    setConfig.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    DescriptorSetLayout const setLayout{context.core, setConfig, create};
    Fill const fill{context.core, setLayout, test::shaders::UNIFORM_COLOR_FRAG};
    Target const target{context.core};

    Buffer<std::array<float, 4>> color{
        context.core,
        host_written<std::array<float, 4>>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        1
    };
    color.write(std::array<std::array<float, 4>, 1>{GREEN});

    DescriptorPool const pool{context.core, DescriptorPool::Config{{&setLayout}}, create};
    DescriptorSet const set{pool, setLayout, create};
    set.write({0, color});

    test::record_and_submit(context, [&](PrimaryCmdBuffer& cmd) {
        record_fill(context, cmd, fill, target, [&](BaseCmdBuffer& c) { c.bindDescriptorSet(set, fill.layout, 0); });
    });

    expect_green(target);
}

CVK_TEST(DescriptorDraw, bindless_texture_written_after_bind) {
    test::Context const context{};
    constexpr uint32_t INDEX = 17;

    DescriptorSetLayout::Config setConfig{};
    setConfig.add(
        0,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        64,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
            | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
    );
    DescriptorSetLayout const setLayout{context.core, setConfig, create};
    Fill const fill{
        context.core,
        setLayout,
        test::shaders::BINDLESS_FRAG,
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
    Buffer<unsigned char> staging{context.core, host_written<unsigned char>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT), 4};
    staging.write(GREEN_PIXEL);

    DescriptorPool const pool{context.core, {{&setLayout, 1, 32}}, create};
    DescriptorSet const set{pool, setLayout, 32, create};

    CmdPool const cmdPool{context.core, CmdPool::Config{context.core.queue(0)}, create};
    PrimaryCmdBuffer cmd{cmdPool, create};
    cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    Barrier toTransfer{};
    toTransfer.addTransition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
    toTransfer.execute(cmd);
    texture.copy(cmd, staging);
    Barrier toShader{};
    toShader.addTransition(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    toShader.execute(cmd);

    record_fill(context, cmd, fill, target, [&](BaseCmdBuffer& c) {
        c.bindDescriptorSet(set, fill.layout, 0);
        c.pushConstants(fill.layout, VK_SHADER_STAGE_FRAGMENT_BIT, INDEX);
    });
    cmd.end();

    // update after bind: the bind is recorded, the descriptor comes now
    set.write({0, view, sampler, INDEX});
    test::submit_and_wait(context, cmd);

    expect_green(target);
}
}
