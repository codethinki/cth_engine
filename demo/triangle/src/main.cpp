#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/data/image.hpp"
#include "jvk/graph/frame.hpp"
#include "jvk/graph/surface.hpp"
#include "jvk/graph/stage.hpp"
#include "jvk/graph/stage_graph.hpp"

#include "cgfw/window/window.hpp"

#include "cvk/base/cvk.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/pipeline/graphics_pipeline.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/utility/constant/device_constants.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/coro/this_coro.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <stdexcept>
#include <utility>


namespace {
using enum cvk::QueueFamilyProperties;

constexpr VkExtent2D WINDOW_EXTENT{800, 600};

/**
 * format of the swapchain and the pipeline, fixed: the pipeline is declared before the surface, see `run()`
 */
constexpr VkSurfaceFormatKHR SURFACE_FORMAT{
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
};

constexpr VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;

constexpr size_t QUEUE_IDX = 0; ///< the graphics queue that presents
constexpr size_t TRIANGLE_STAGE_ID = 0;

/** after a frame without an image, e.g. minimized */
constexpr auto IDLE_WAIT = std::chrono::milliseconds{16};

std::filesystem::path const SHADER_DIR{TRIANGLE_SHADER_DIR};

/**
 * the multisampled color image of the frames, the rendering resolves it into the swapchain image
 */
struct MsaaTarget {
    MsaaTarget(jvk::Core const& core, VkExtent2D extent) :
        image{
            core,
            {
                .format = SURFACE_FORMAT.format,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                .samples = MSAA_SAMPLES,
                .memory = {.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            },
            extent
        },
        view{image, {}, cvk::create} {}

    jvk::Image image;
    cvk::ImageView view;
};

/**
 * draws the triangle in every frame until the window closes
 * @details only the scopes: the `jvk::scoped_submit` ends first and submits, then the `jvk::scoped_acquire` presents
 */
cth::co::executor_task<void> draw_frames(
    cgfw::Window& window,
    jvk::Core const& core,
    jvk::StageGraph& graph,
    jvk::Surface& surface,
    cvk::GraphicsPipeline const& pipeline,
    std::optional<MsaaTarget>& msaaOpt
) {
    while(!window.shouldClose()) {
        window.pollEvents();
        auto& stage = (co_await graph.nextFrame()).stage(TRIANGLE_STAGE_ID);

        auto const image = co_await surface.acquire(
            stage,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
        );
        // extent 0: no image, the next nextFrame() skips the stage
        if(!image) {
            co_await cth::co::this_coro::wait(IDLE_WAIT);
            continue;
        }

        // a new extent: the frame keeps the old target until the gpu finished it
        if(!msaaOpt.has_value() || msaaOpt->image.extent() != image->extent) {
            if(msaaOpt.has_value()) {
                stage.retain(std::move(msaaOpt->view));
                stage.retain(std::move(msaaOpt->image));
            }
            msaaOpt.emplace(core, image->extent);
        }

        auto const submit = stage.begin();
        auto const rendering = submit->beginRendering(
            {
                .colorAttachments = {
                    {
                        .view = msaaOpt->view,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        .resolveViewPtr = &*image->view,
                        .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
                    }
                },
            }
        );
        submit->bind(pipeline);
        submit->setViewport(image->extent);
        submit->setScissor(image->extent);
        submit->draw(3);
    }
}

/**
 * creates the window, the vulkan objects and the pipeline, then draws until the window closes
 * @details the declaration order is the destruction order backwards:
 * - the pipeline and the multisampled target live until `~StageGraph()` waited for the gpu
 * - the surface is destroyed before the graph: its present jobs wait for the semaphores of the graph
 */
void run() {
    cvk::Cvk const cvk{};
    cgfw::Window window{{.title = "triangle", .extent = WINDOW_EXTENT}, cgfw::create};

    // the surface extensions of the window and the instance extensions of a present queue
    auto extensions = cgfw::instance_extensions();
    for(auto const extension : cvk::constants::PRESENT_INSTANCE_EXTENSIONS)
        if(!std::ranges::contains(extensions, extension))
            extensions.emplace_back(extension);
    cvk::Instance const instance{
        {.appName = "triangle", .extensions = std::move(extensions)},
        cvk::create
    };

    // the core picks a queue that presents to the surface, the jvk surface takes it
    auto windowSurface = window.surface(instance);
    jvk::Core core{instance, {.vk = {.queueProperties = {GRAPHICS | PRESENT}}}, {windowSurface}};
    cvk::Core const& vk = core;
    if((vk.physicalDevice().limits().framebufferColorSampleCounts & MSAA_SAMPLES) == 0)
        throw std::runtime_error{
            std::format("the device offers no {}x msaa for color attachments", +MSAA_SAMPLES)
        };

    cvk::Shader const vertex{core, VK_SHADER_STAGE_VERTEX_BIT, SHADER_DIR / "triangle.vert.spv"};
    cvk::Shader const fragment{
        core,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        SHADER_DIR / "triangle.frag.spv"
    };
    cvk::GraphicsPipeline::Config pipelineConfig{.colorFormats = {SURFACE_FORMAT.format}};
    pipelineConfig.multisample.rasterizationSamples = MSAA_SAMPLES;
    pipelineConfig.addStage(vertex).addStage(fragment);
    cvk::GraphicsPipeline const pipeline{core, std::move(pipelineConfig), cvk::create};

    std::optional<MsaaTarget> msaaOpt{};
    jvk::StageGraph graph{core, {.stages = {{.queueIdx = QUEUE_IDX}}}};
    jvk::Surface surface{
        core,
        core.queue(QUEUE_IDX),
        {.swapchain = {.formats = {SURFACE_FORMAT}}},
        std::move(windowSurface),
    };

    core.executor().sync_wait(draw_frames(window, core, graph, surface, pipeline, msaaOpt));
}
}

int main() {
    try { run(); }
    catch(std::exception const& e) {
        std::println(stderr, "triangle failed: {}", e.what());
        return 1;
    }
}
