#include "timgui/imgui.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/core.hpp"
#include "jvk/data/buffer.hpp"
#include "jvk/data/image.hpp"
#include "jvk/data/resource_heap.hpp"
#include "jvk/graph/frame.hpp"
#include "jvk/graph/stage.hpp"
#include "jvk/graph/stage_graph.hpp"
#include "jvk/sync/resource_states.hpp"

#include "cgfw/window/window.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/data/buffer/buffer.hpp"
#include "cvk/data/descriptor/image_descriptor.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/cmd/rendering_info.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <vk_mem_alloc.h>
#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>


namespace timgui {
namespace {
    constexpr VkExtent2D EXTENT = test::WINDOW_EXTENT;

    constexpr ImU32 RED = IM_COL32(255, 0, 0, 255);
    constexpr ImU32 GRAY = IM_COL32(128, 128, 128, 255);
    constexpr uint32_t CLEAR = 0; ///< transparent black

    constexpr cvk::BaseBuffer::Config STAGING_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
    };

    constexpr cvk::BaseBuffer::Config READBACK_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT},
    };

    constexpr jvk::BufferAccess HOST_READ{VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};

    /**
     * color attachment of the draws and the buffer it is copied to
     */
    struct Target {
        Target(jvk::Core const& core, VkFormat format) :
            image{
                core,
                {
                    .format = format,
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                },
                EXTENT
            },
            view{image, {}, cvk::create},
            readback{core, READBACK_CONFIG, size_t{EXTENT.width} * EXTENT.height} {}

        /**
         * @return RGBA8 of the pixel, R in the lowest byte
         * @pre the gpu finished the copy into `readback`
         */
        [[nodiscard]] uint32_t pixel(uint32_t x, uint32_t y) const {
            auto const mapping = readback.map();
            readback.invalidate();
            return mapping[size_t{y} * EXTENT.width + x];
        }

        jvk::Image image;
        cvk::ImageView view;
        cvk::Buffer<uint32_t> readback;
    };

    /**
     * @return the byte of the channel, `0` is R
     */
    [[nodiscard]] constexpr int channel(uint32_t pixel, uint32_t channelIdx) {
        return static_cast<int>((pixel >> (8 * channelIdx)) & 0xFF);
    }

    cth::co::executor_task<void> await_stage(jvk::Stage const& stage) { co_await stage; }

    /**
     * @return stage 0 of the next frame
     */
    [[nodiscard]] jvk::Stage& next_stage(jvk::Core const& core, jvk::StageGraph& graph) {
        return core.executor().sync_wait(graph.nextFrame()).stage(0);
    }

    /**
     * submits the uploads of the draw data in the next frame
     */
    void upload_frame(
        jvk::Core const& core,
        jvk::StageGraph& graph,
        Imgui& imgui,
        ImDrawData& drawData
    ) {
        auto& stage = next_stage(core, graph);
        auto const submit = stage.begin();
        imgui.upload(stage, *submit, drawData);
    }

    /**
     * uploads and draws the draw data into the cleared target in the next frame
     * @details copies the target to its buffer, waits until the gpu finished
     * @param depthStencilViewPtr `nullptr` -> no depth and no stencil attachment, else both
     */
    void draw_frame(
        jvk::Core const& core,
        jvk::StageGraph& graph,
        Imgui& imgui,
        ImDrawData& drawData,
        Target const& target,
        cvk::ImageView const* depthStencilViewPtr = nullptr
    ) {
        cvk::RenderingInfo info{
            .colorAttachments = {
                cvk::RenderingAttachment{
                    .view = target.view,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                },
            },
        };
        if(depthStencilViewPtr != nullptr) {
            cvk::RenderingAttachment const depthStencil{
                .view = *depthStencilViewPtr,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .clearValue = {.depthStencil = {.depth = 1.f, .stencil = 0}},
            };
            info.depthAttachment = depthStencil;
            info.stencilAttachment = depthStencil;
        }

        auto& stage = next_stage(core, graph);
        {
            auto const submit = stage.begin();
            imgui.upload(stage, *submit, drawData);
            {
                auto const rendering = submit->beginRendering(info);
                imgui.draw(stage, *submit, drawData);
            }
            target.image.copyTo(*submit, target.readback);
            submit->use(target.readback, HOST_READ);
        }
        core.executor().sync_wait(await_stage(stage));
    }

    /**
     * ImGui frame without a window, in the current context: `DisplaySize` is `EXTENT`
     * @param draw records into the foreground draw list
     * @return the draw data of the frame
     */
    template<class Record>
    [[nodiscard]] ImDrawData& windowless_frame(Record const& draw) {
        auto& io = ImGui::GetIO();
        io.DisplaySize = ImVec2{
            static_cast<float>(EXTENT.width),
            static_cast<float>(EXTENT.height)
        };
        io.DeltaTime = 1.f / 60.f;

        ImGui::NewFrame();
        draw(*ImGui::GetForegroundDrawList());
        ImGui::Render();
        return *ImGui::GetDrawData();
    }

    /**
     * a rectangle over the whole display
     */
    [[nodiscard]] ImDrawData& rect_frame(ImU32 color) {
        return windowless_frame(
            [color](ImDrawList& drawList) {
                drawList.AddRectFilled(ImVec2{0.f, 0.f}, ImGui::GetIO().DisplaySize, color);
            }
        );
    }
}

TIMGUI_TEST(Imgui, constructor_makes_the_contexts_current) {
    test::Context context{};
    cgfw::Window const window{test::window_config()};

    Imgui const imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};

    ASSERT_NE(ImGui::GetCurrentContext(), nullptr);
    EXPECT_NE(ImPlot::GetCurrentContext(), nullptr);
    auto const& io = ImGui::GetIO();
    EXPECT_NE(io.BackendFlags & ImGuiBackendFlags_RendererHasTextures, 0);
    EXPECT_NE(io.BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset, 0);
    EXPECT_EQ(io.IniFilename, nullptr);
}

TIMGUI_TEST(Imgui, destructor_destroys_the_contexts) {
    test::Context context{};
    cgfw::Window const window{test::window_config()};

    { Imgui const imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}}; }

    // the destruction of the current contexts clears the current contexts
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
    EXPECT_EQ(ImPlot::GetCurrentContext(), nullptr);
}

TIMGUI_TEST(Imgui, upload_creates_updates_and_destroys_a_texture) {
    test::Context context{};
    cgfw::Window const window{test::window_config()};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};
    jvk::StageGraph graph{context.core, {.stages = {{.queueIdx = 0}}}};

    ImTextureData data{};
    data.Create(ImTextureFormat_RGBA32, 4, 4);
    ImVector<ImTextureData*> textures{};
    textures.push_back(&data);
    ImDrawData drawData{};
    drawData.Textures = &textures;

    upload_frame(context.core, graph, imgui, drawData);
    EXPECT_EQ(data.Status, ImTextureStatus_OK);
    auto const id = data.GetTexID();
    EXPECT_NE(id, ImTextureID_Invalid);

    data.SetStatus(ImTextureStatus_WantUpdates);
    upload_frame(context.core, graph, imgui, drawData);
    EXPECT_EQ(data.Status, ImTextureStatus_OK);
    EXPECT_EQ(data.GetTexID(), id);

    // a destroy that ImGui plans
    data.WantDestroyNextFrame = true;
    data.SetStatus(ImTextureStatus_WantDestroy);
    upload_frame(context.core, graph, imgui, drawData);
    EXPECT_EQ(data.Status, ImTextureStatus_Destroyed);
    EXPECT_EQ(data.GetTexID(), ImTextureID_Invalid);
}

TIMGUI_TEST(Imgui, draw_fills_with_the_vertex_color) {
    test::Context context{};
    cgfw::Window const window{test::window_config()};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};
    Target const target{context.core, VK_FORMAT_R8G8B8A8_UNORM};
    jvk::StageGraph graph{context.core, {.stages = {{.queueIdx = 0}}}};

    auto& drawData = rect_frame(RED);
    draw_frame(context.core, graph, imgui, drawData, target);

    EXPECT_EQ(target.pixel(EXTENT.width / 2, EXTENT.height / 2), RED);
}

TIMGUI_TEST(Imgui, draw_into_a_depth_stencil_rendering) {
    constexpr std::array CANDIDATES{
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };

    test::Context context{};
    cvk::Core const& vk = context.core;
    VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;
    try {
        depthStencilFormat = vk.physicalDevice().findSupportedFormat(
            CANDIDATES,
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }
    catch(cvk::cvk_exception const&) { GTEST_SKIP() << "no depth stencil format"; }

    cgfw::Window const window{test::window_config()};
    Imgui imgui{
        context.core,
        window,
        {
            .colorFormat = VK_FORMAT_R8G8B8A8_UNORM,
            .depthFormat = depthStencilFormat,
            .stencilFormat = depthStencilFormat,
        }
    };
    Target const target{context.core, VK_FORMAT_R8G8B8A8_UNORM};
    jvk::Image const depthStencil{
        context.core,
        {.format = depthStencilFormat, .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
        EXTENT
    };
    cvk::ImageView const depthStencilView{depthStencil, {}, cvk::create};
    jvk::StageGraph graph{context.core, {.stages = {{.queueIdx = 0}}}};

    auto& drawData = rect_frame(RED);
    draw_frame(context.core, graph, imgui, drawData, target, &depthStencilView);

    // no depth test: the cleared depth does not hide the draw
    EXPECT_EQ(target.pixel(EXTENT.width / 2, EXTENT.height / 2), RED);
}

TIMGUI_TEST(Imgui, srgb_target_keeps_the_srgb_color) {
    test::Context context{};
    cgfw::Window const window{test::window_config()};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_SRGB}};
    Target const target{context.core, VK_FORMAT_R8G8B8A8_SRGB};
    jvk::StageGraph graph{context.core, {.stages = {{.queueIdx = 0}}}};

    auto& drawData = rect_frame(GRAY);
    draw_frame(context.core, graph, imgui, drawData, target);

    // without the conversion to linear the attachment would store 188
    auto const pixel = target.pixel(EXTENT.width / 2, EXTENT.height / 2);
    for(uint32_t channelIdx = 0; channelIdx < 3; ++channelIdx)
        EXPECT_LE(std::abs(channel(pixel, channelIdx) - 128), 1) << "channel " << channelIdx;
    EXPECT_EQ(channel(pixel, 3), 255);
}

TIMGUI_TEST(Imgui, draw_clips_to_the_clip_rect) {
    test::Context context{};
    cgfw::Window const window{test::window_config()};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};
    Target const target{context.core, VK_FORMAT_R8G8B8A8_UNORM};
    jvk::StageGraph graph{context.core, {.stages = {{.queueIdx = 0}}}};

    auto& drawData = windowless_frame(
        [](ImDrawList& drawList) {
            auto const display = ImGui::GetIO().DisplaySize;
            drawList.PushClipRect(ImVec2{0.f, 0.f}, ImVec2{display.x / 2, display.y});
            drawList.AddRectFilled(ImVec2{0.f, 0.f}, display, RED);
            drawList.PopClipRect();
        }
    );
    draw_frame(context.core, graph, imgui, drawData, target);

    EXPECT_EQ(target.pixel(EXTENT.width / 4, EXTENT.height / 2), RED);
    EXPECT_EQ(target.pixel(EXTENT.width * 3 / 4, EXTENT.height / 2), CLEAR);
}

TIMGUI_TEST(Imgui, draw_records_an_implot_plot) {
    constexpr std::array<float, 3> VALUES{0.f, 1.f, 0.5f};

    test::Context context{};
    cgfw::Window const window{test::window_config()};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};
    Target const target{context.core, VK_FORMAT_R8G8B8A8_UNORM};
    jvk::StageGraph graph{context.core, {.stages = {{.queueIdx = 0}}}};

    auto& drawData = windowless_frame(
        [&](ImDrawList&) {
            ImGui::SetNextWindowPos(ImVec2{0.f, 0.f});
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("plot");
            if(ImPlot::BeginPlot("values")) {
                ImPlot::PlotLine("line", VALUES.data(), static_cast<int>(VALUES.size()));
                ImPlot::EndPlot();
            }
            ImGui::End();
        }
    );
    draw_frame(context.core, graph, imgui, drawData, target);

    // the window background at least
    EXPECT_NE(target.pixel(EXTENT.width / 2, EXTENT.height / 2), CLEAR);
}

TIMGUI_TEST(Imgui, draw_without_vertices_records_nothing) {
    test::Context context{};
    cgfw::Window const window{test::window_config()};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};
    Target const target{context.core, VK_FORMAT_R8G8B8A8_UNORM};
    jvk::StageGraph graph{context.core, {.stages = {{.queueIdx = 0}}}};

    auto& drawData = windowless_frame([](ImDrawList&) {});
    ASSERT_EQ(drawData.TotalVtxCount, 0);
    draw_frame(context.core, graph, imgui, drawData, target);

    EXPECT_EQ(target.pixel(EXTENT.width / 2, EXTENT.height / 2), CLEAR);
}

TIMGUI_TEST(Imgui, texture_draws_an_image_of_the_heap) {
    constexpr std::array<uint8_t, 4> GREEN{0, 255, 0, 255};

    test::Context context{};
    cgfw::Window const window{test::window_config()};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};
    Target const target{context.core, VK_FORMAT_R8G8B8A8_UNORM};
    jvk::Image const image{
        context.core,
        {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        },
        VkExtent2D{1, 1}
    };
    auto const sampled = context.core.resourceHeap().insert(cvk::SampledImageDescriptor{image});
    jvk::StageGraph graph{context.core, {.stages = {{.queueIdx = 0}}}};

    // the image: one green pixel, ready for the fragment shader
    {
        jvk::Buffer<uint8_t> staging{context.core, STAGING_CONFIG, GREEN.size()};
        staging.write(GREEN);
        auto& stage = next_stage(context.core, graph);
        auto const submit = stage.begin();
        image.copy(*submit, staging);
        submit->use(sampled, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
        stage.retain(std::move(staging));
    }

    auto& drawData = windowless_frame(
        [&](ImDrawList& drawList) {
            drawList.AddImage(
                imgui.texture(sampled.idx()),
                ImVec2{0.f, 0.f},
                ImGui::GetIO().DisplaySize
            );
        }
    );
    draw_frame(context.core, graph, imgui, drawData, target);

    EXPECT_EQ(target.pixel(EXTENT.width / 2, EXTENT.height / 2), IM_COL32(0, 255, 0, 255));
}

TIMGUI_TEST(Frame, passes_the_extent_and_the_first_delta_time) {
    TIMGUI_SKIP_WITHOUT_DISPLAY();
    test::Context context{};
    cgfw::Window window{test::window_config(), cgfw::create};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};

    window.pollEvents();
    auto frame = imgui.frame();
    auto const extent = window.extent();
    auto const& io = ImGui::GetIO();
    EXPECT_EQ(io.DisplaySize.x, static_cast<float>(extent.width));
    EXPECT_EQ(io.DisplaySize.y, static_cast<float>(extent.height));
    EXPECT_EQ(io.DeltaTime, 1.f / 60.f);

    auto const& drawData = frame.render();
    EXPECT_EQ(drawData.DisplaySize.x, io.DisplaySize.x);
    EXPECT_EQ(drawData.DisplaySize.y, io.DisplaySize.y);
}

TIMGUI_TEST(Frame, next_frame_passes_the_time_since_the_last) {
    TIMGUI_SKIP_WITHOUT_DISPLAY();
    test::Context context{};
    cgfw::Window window{test::window_config(), cgfw::create};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};

    window.pollEvents();
    { auto const frame = imgui.frame(); }
    window.pollEvents();
    auto const frame = imgui.frame();

    EXPECT_GT(ImGui::GetIO().DeltaTime, 0.f);
    EXPECT_EQ(ImGui::GetFrameCount(), 2);
}

TIMGUI_TEST(Frame, destructor_without_render_ends_the_frame) {
    TIMGUI_SKIP_WITHOUT_DISPLAY();
    test::Context context{};
    cgfw::Window window{test::window_config(), cgfw::create};
    Imgui imgui{context.core, window, {.colorFormat = VK_FORMAT_R8G8B8A8_UNORM}};

    window.pollEvents();
    {
        auto const frame = imgui.frame();
        EXPECT_TRUE(ImGui::GetCurrentContext()->WithinFrameScope);
    }

    EXPECT_FALSE(ImGui::GetCurrentContext()->WithinFrameScope);
}
}
