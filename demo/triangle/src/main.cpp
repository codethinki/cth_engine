#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/graph/frame.hpp"
#include "jvk/graph/presenter.hpp"
#include "jvk/graph/stage.hpp"
#include "jvk/graph/stage_graph.hpp"

#include "cgfw/window/window.hpp"

#include "cvk/base/cvk.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/render/pipeline/graphics_pipeline.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/utility/constant/device_constants.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/coro/this_coro.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <print>
#include <utility>


namespace {
using enum cvk::QueueFamilyProperties;

constexpr VkExtent2D WINDOW_EXTENT{800, 600};

/**
 * format of the swapchain and the pipeline, fixed: the pipeline is declared before the presenter, see `run()`
 */
constexpr VkSurfaceFormatKHR SURFACE_FORMAT{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

constexpr size_t QUEUE_IDX = 0; ///< the graphics queue that presents
constexpr size_t TRIANGLE_STAGE_ID = 0;

constexpr auto IDLE_WAIT = std::chrono::milliseconds{16}; ///< after a frame without an image, e.g. minimized

std::filesystem::path const SHADER_DIR{TRIANGLE_SHADER_DIR};

/**
 * draws the triangle in every frame until the window closes
 * @details only the scopes: the `jvk::scoped_submit` ends first and submits, then the `jvk::scoped_acquire` presents
 */
cth::co::executor_task<void> draw_frames(
    cgfw::Window const& window,
    jvk::StageGraph& graph,
    jvk::Presenter& presenter,
    cvk::GraphicsPipeline const& pipeline
) {
    while(!window.shouldClose()) {
        window.pollEvents();
        auto& stage = (co_await graph.nextFrame()).stage(TRIANGLE_STAGE_ID);

        auto const image = co_await presenter.acquire(stage, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
        // extent 0: no image, the next nextFrame() skips the stage
        if(!image) {
            co_await cth::co::this_coro::wait(IDLE_WAIT);
            continue;
        }

        auto const submit = stage.begin();
        auto const rendering = submit->beginRendering(
            {.colorAttachments = {{.view = image->view, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR}}}
        );
        submit->bindPipeline(pipeline);
        submit->setViewport(image->extent);
        submit->setScissor(image->extent);
        submit->draw(3);
    }
}

/**
 * creates the window, the vulkan objects and the pipeline, then draws until the window closes
 * @details the declaration order is the destruction order backwards:
 * - the pipeline lives until `~StageGraph()` waited for the gpu
 * - the presenter is destroyed before the graph: its present jobs wait for the semaphores of the graph
 */
void run() {
    cvk::Cvk const cvk{};
    cgfw::Window const window{{.title = "triangle", .extent = WINDOW_EXTENT}, cgfw::create};

    // the surface extensions of the window and the instance extensions of a present queue
    auto extensions = cgfw::instance_extensions();
    for(auto const extension : cvk::constants::PRESENT_INSTANCE_EXTENSIONS)
        if(!std::ranges::contains(extensions, extension)) extensions.emplace_back(extension);
    cvk::Instance const instance{{.appName = "triangle", .extensions = std::move(extensions)}, cvk::create};

    // the core picks a queue that presents to the surface, the presenter takes the surface
    auto surface = window.surface(instance);
    jvk::Core core{instance, {.vk = {.queueProperties = {GRAPHICS | PRESENT}}}, {surface}};

    cvk::Shader const vertex{core, VK_SHADER_STAGE_VERTEX_BIT, SHADER_DIR / "triangle.vert.spv"};
    cvk::Shader const fragment{core, VK_SHADER_STAGE_FRAGMENT_BIT, SHADER_DIR / "triangle.frag.spv"};
    cvk::PipelineLayout const layout{core, {}, cvk::create};
    cvk::GraphicsPipeline::Config pipelineConfig{.colorFormats = {SURFACE_FORMAT.format}};
    pipelineConfig.addStage(vertex).addStage(fragment);
    cvk::GraphicsPipeline const pipeline{layout, std::move(pipelineConfig), cvk::create};

    jvk::StageGraph graph{core, {.stages = {{.queueIdx = QUEUE_IDX}}}};
    jvk::Presenter presenter{
        core,
        core.queue(QUEUE_IDX),
        {.swapchain = {.formats = {SURFACE_FORMAT}}},
        std::move(surface),
    };

    core.executor().sync_wait(draw_frames(window, graph, presenter, pipeline));
}
}

int main() {
    try {
        run();
    }
    catch(std::exception const& e) {
        std::println(stderr, "triangle failed: {}", e.what());
        return 1;
    }
}
