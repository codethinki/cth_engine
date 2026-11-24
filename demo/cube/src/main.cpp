#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/data/buffer.hpp"
#include "jvk/data/heap_idx.hpp"
#include "jvk/data/image.hpp"
#include "jvk/data/resource_heap.hpp"
#include "jvk/data/sampler_heap.hpp"
#include "jvk/graph/frame.hpp"
#include "jvk/graph/surface.hpp"
#include "jvk/graph/stage.hpp"
#include "jvk/graph/stage_graph.hpp"

#include "timgui/imgui.hpp"

#include "cgfw/window/window.hpp"

#include "cvk/base/cvk.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/buffer/device_ptr.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/pipeline/graphics_pipeline.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/utility/constant/device_constants.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <volk.h>

#include <cth/chrono.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/coro/this_coro.hpp>
#include <cth/io/log.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


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
constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;

constexpr size_t QUEUE_IDX = 0; ///< the graphics queue that presents
constexpr size_t CUBE_STAGE_ID = 0;

/** after a frame without an image, e.g. minimized */
constexpr auto IDLE_WAIT = std::chrono::milliseconds{16};

constexpr float SPEED = 1.f; ///< radians per second
glm::vec3 const AXIS = glm::normalize(glm::vec3{0.3f, 1.f, 0.2f});

constexpr uint32_t TEXTURE_SIZE = 256;
/** full mip chain */
constexpr uint32_t TEXTURE_MIP_LEVELS = cvk::mip_level_count({TEXTURE_SIZE, TEXTURE_SIZE});
constexpr uint32_t CHECKER_SIZE = 32; ///< pixels per square of the checkerboard

std::filesystem::path const SHADER_DIR{CUBE_SHADER_DIR};

/**
 * vertex of the cube, `Vertex` of cube.glsl in scalar layout
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};
static_assert(sizeof(Vertex) == 32);
static_assert(offsetof(Vertex, normal) == 12 && offsetof(Vertex, uv) == 24);

/**
 * push data of the draw, `Draw` of cube.glsl in scalar layout
 */
struct Draw {
    cvk::device_ptr<Vertex> vertices;
    glm::mat4 mvp;
    glm::mat3 normal;
    jvk::heap_idx<jvk::SampledImage> texture;
    jvk::heap_idx<jvk::Sampler> sampler;
};
static_assert(offsetof(Draw, mvp) == 8);
static_assert(offsetof(Draw, normal) == 72);
static_assert(offsetof(Draw, texture) == 108);
static_assert(offsetof(Draw, sampler) == 112);
static_assert(sizeof(Draw) <= 256, "the push data of the draw must fit into maxPushDataSize");

/**
 * the 24 vertices of the cube, 4 per face, counter-clockwise seen from outside
 */
std::array<Vertex, 24> cube_vertices() {
    // normal, then u and v of the face with u x v = normal
    constexpr std::array<std::array<glm::vec3, 3>, 6> FACES{
        {
            {glm::vec3{1, 0, 0}, glm::vec3{0, 0, -1}, glm::vec3{0, 1, 0}},
            {glm::vec3{-1, 0, 0}, glm::vec3{0, 0, 1}, glm::vec3{0, 1, 0}},
            {glm::vec3{0, 1, 0}, glm::vec3{1, 0, 0}, glm::vec3{0, 0, -1}},
            {glm::vec3{0, -1, 0}, glm::vec3{1, 0, 0}, glm::vec3{0, 0, 1}},
            {glm::vec3{0, 0, 1}, glm::vec3{1, 0, 0}, glm::vec3{0, 1, 0}},
            {glm::vec3{0, 0, -1}, glm::vec3{-1, 0, 0}, glm::vec3{0, 1, 0}},
        }
    };
    // corners in u and v, the texture coordinates go down in v
    constexpr std::array<glm::vec2, 4> CORNERS{
        glm::vec2{-1, -1},
        glm::vec2{1, -1},
        glm::vec2{1, 1},
        glm::vec2{-1, 1}
    };

    std::array<Vertex, 24> vertices{};
    for(size_t faceIdx = 0; faceIdx < FACES.size(); ++faceIdx) {
        auto const& [normal, u, v] = FACES[faceIdx];
        for(size_t cornerIdx = 0; cornerIdx < CORNERS.size(); ++cornerIdx) {
            auto const corner = CORNERS[cornerIdx];
            vertices[faceIdx * 4 + cornerIdx] = Vertex{
                .position = 0.5f * (normal + corner.x * u + corner.y * v),
                .normal = normal,
                .uv = glm::vec2{(corner.x + 1.f) * 0.5f, (1.f - corner.y) * 0.5f},
            };
        }
    }
    return vertices;
}

/**
 * two triangles per face
 */
std::array<uint16_t, 36> cube_indices() {
    std::array<uint16_t, 36> indices{};
    for(uint16_t faceIdx = 0; faceIdx < 6; ++faceIdx) {
        auto const first = static_cast<uint16_t>(faceIdx * 4);
        std::array<uint16_t, 6> const corners{0, 1, 2, 2, 3, 0};
        for(size_t i = 0; i < corners.size(); ++i)
            indices[faceIdx * 6 + i] = static_cast<uint16_t>(first + corners[i]);
    }
    return indices;
}

/**
 * RGBA8 checkerboard of two colors
 */
std::vector<uint8_t> checkerboard() {
    std::vector<uint8_t> pixels(size_t{TEXTURE_SIZE} * TEXTURE_SIZE * 4);
    for(uint32_t y = 0; y < TEXTURE_SIZE; ++y)
        for(uint32_t x = 0; x < TEXTURE_SIZE; ++x) {
            bool const light = (x / CHECKER_SIZE + y / CHECKER_SIZE) % 2 == 0;
            std::array<uint8_t, 4> const color = light
                ? std::array<uint8_t, 4>{240, 200, 80, 255}
                : std::array<uint8_t, 4>{40, 70, 140, 255};
            std::ranges::copy(color, pixels.begin() + (size_t{y} * TEXTURE_SIZE + x) * 4);
        }
    return pixels;
}

/**
 * the multisampled color and depth images of the frames, the rendering resolves the color into the swapchain image
 */
struct Targets {
    Targets(jvk::Core const& core, VkExtent2D extent) :
        color{
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
        depth{
            core,
            {
                .format = DEPTH_FORMAT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                .samples = MSAA_SAMPLES,
                .memory = {.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            },
            extent
        },
        colorView{color, {}, cvk::create},
        depthView{depth, {}, cvk::create} {}

    jvk::Image color;
    jvk::Image depth;
    cvk::ImageView colorView;
    cvk::ImageView depthView;
};

/**
 * config of a device-local buffer that a copy fills
 */
constexpr cvk::BaseBuffer::Config device_buffer(VkBufferUsageFlags usage) {
    return {
        .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memory = {.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
    };
}

/**
 * the cube: the vertices and the indices on the gpu, the texture with its mips, the heap handles
 * @details `upload()` records the copies in the first frame
 */
struct Cube {
    explicit Cube(jvk::Core const& core) :
        vertices{core, device_buffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT), 24},
        indices{core, device_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT), 36},
        texture{
            core,
            {
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .mipLevels = TEXTURE_MIP_LEVELS,
            },
            VkExtent2D{TEXTURE_SIZE, TEXTURE_SIZE}
        },
        sampled{core.resourceHeap().insert(cvk::SampledImageDescriptor{texture})},
        sampler{core.samplerHeap().insert(cvk::SamplerConfig{})} {}

    /**
     * records the uploads through staging buffers, the stage keeps them until the gpu finished
     */
    void upload(jvk::Stage& stage, jvk::PrimaryCmdBuffer& cmd) const {
        cvk::Core const& vk = cmd.core();

        jvk::Buffer<Vertex> vertexStaging{vk, STAGING, 24};
        vertexStaging.write(cube_vertices());
        vertices.copy(cmd, vertexStaging);

        jvk::Buffer<uint16_t> indexStaging{vk, STAGING, 36};
        indexStaging.write(cube_indices());
        indices.copy(cmd, indexStaging);

        auto const pixels = checkerboard();
        jvk::Buffer<uint8_t> textureStaging{vk, STAGING, pixels.size()};
        textureStaging.write(pixels);
        texture.copy(cmd, textureStaging);
        texture.blitMipLevels(cmd);

        stage.retain(std::move(vertexStaging));
        stage.retain(std::move(indexStaging));
        stage.retain(std::move(textureStaging));
    }

    static constexpr cvk::BaseBuffer::Config STAGING{
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
    };

    jvk::Buffer<Vertex> vertices;
    jvk::Buffer<uint16_t> indices;
    jvk::Image texture;
    jvk::SampledImage sampled;
    jvk::Sampler sampler;
};

//IMPLEMENT moves into an engine library
/**
 * frame times of the last `CAPACITY` frames, in ms
 * @details ring buffer: full -> a new time overwrites the oldest one
 */
class FrameTimes {
public:
    static constexpr size_t CAPACITY = 240;

    /**
     * stores the time of a frame, in ms
     * @details full -> overwrites the oldest time
     */
    constexpr void add(cth::chrono::clock_t::duration frameTime) {
        _times[_nextIdx] = std::chrono::duration<float, std::milli>{frameTime}.count();
        _nextIdx = (_nextIdx + 1) % CAPACITY;
        _count = std::min(_count + 1, CAPACITY);
    }

    /**
     * @return the stored time at the rank `fraction * (count - 1)` of the sorted times
     * @details the rank is rounded down
     * @pre `!empty()`
     * @pre `0 <= fraction <= 1`
     */
    [[nodiscard]] constexpr float percentile(float fraction) const {
        CTH_CRITICAL(empty(), "no frame time is stored") {}
        CTH_CRITICAL(
            fraction < 0.f || fraction > 1.f,
            "the fraction must be in [0, 1], got {}",
            fraction
        ) {}

        auto sorted = _times;
        auto const stored = std::span{sorted}.first(_count);
        auto const rank = static_cast<size_t>(fraction * static_cast<float>(_count - 1));
        std::ranges::nth_element(stored, stored.begin() + static_cast<ptrdiff_t>(rank));
        return stored[rank];
    }

    [[nodiscard]] constexpr float median() const { return percentile(0.5f); }
    [[nodiscard]] constexpr float p75() const { return percentile(0.75f); }
    [[nodiscard]] constexpr float p99() const { return percentile(0.99f); }

    /**
     * @return the stored times in storage order, the oldest at `oldestIdx()`
     */
    [[nodiscard]] constexpr std::span<float const> values() const {
        return std::span{_times}.first(_count);
    }

    /**
     * @return index of the oldest time in `values()`
     */
    [[nodiscard]] constexpr size_t oldestIdx() const { return _count < CAPACITY ? 0 : _nextIdx; }

    [[nodiscard]] constexpr bool empty() const { return _count == 0; }

private:
    std::array<float, CAPACITY> _times{};
    size_t _nextIdx = 0; ///< of the next `add()`
    size_t _count = 0;
};

constexpr float FRAME_TIME_WINDOW_WIDTH = 360.f; ///< at the first frame, the user can resize it
constexpr float FRAME_TIME_PLOT_HEIGHT = 120.f;

/**
 * ImGui window of the frame times: the percentiles and a plot of the stored times
 * @details the collapsed window shows the percentiles in its title
 * @param[out] collapsed the window was collapsed in the last frame, gets the state of this frame
 */
void frame_time_window(FrameTimes const& times, bool& collapsed) {
    // ImGui knows the collapsed state only inside Begin(): the title follows one frame later
    auto const title = collapsed && !times.empty()
        ? std::format(
            "frame time (med {:.2f}ms, p75 {:.2f}ms, p99 {:.2f}ms)###frame_time",
            times.median(),
            times.p75(),
            times.p99()
        )
        : std::string{"frame time###frame_time"};

    ImGui::SetNextWindowSize(ImVec2{FRAME_TIME_WINDOW_WIDTH, 0.f}, ImGuiCond_FirstUseEver);
    bool const visible = ImGui::Begin(title.c_str());
    collapsed = ImGui::IsWindowCollapsed();

    if(visible && !times.empty()) {
        ImGui::TextUnformatted(std::format("med {:.2f}ms", times.median()).c_str());
        ImGui::TextUnformatted(std::format("p75 {:.2f}ms", times.p75()).c_str());
        ImGui::TextUnformatted(std::format("p99 {:.2f}ms", times.p99()).c_str());

        if(ImPlot::BeginPlot(
            "##frame_times",
            ImVec2{-1.f, FRAME_TIME_PLOT_HEIGHT},
            ImPlotFlags_NoLegend | ImPlotFlags_NoMenus
        )) {
            // x: the frames, the newest at 0; y: the ms, labels on the right
            ImPlot::SetupAxes(
                nullptr,
                nullptr,
                ImPlotAxisFlags_None,
                ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Opposite
            );
            ImPlot::SetupAxisLimits(
                ImAxis_X1,
                -static_cast<double>(FrameTimes::CAPACITY),
                0.,
                ImPlotCond_Always
            );

            auto const values = times.values();
            ImPlotSpec spec{};
            spec.Offset = static_cast<int>(times.oldestIdx());
            ImPlot::PlotLine(
                "##ms",
                values.data(),
                static_cast<int>(values.size()),
                1.,
                -static_cast<double>(values.size() - 1),
                spec
            );
            ImPlot::EndPlot();
        }
    }
    ImGui::End();
}

/**
 * draws the spinning cube and the frame time window in every frame until the window closes
 * @details
 * - only the scope: the `jvk::scoped_submit` submits, the job of the stage presents
 * - the ImGui frame comes first: it takes the events also in a frame without an image
 * - frame time: the time between the starts of two iterations, the first iteration has none
 */
cth::co::executor_task<void> draw_frames(
    cgfw::Window& window,
    jvk::Core const& core,
    jvk::StageGraph& graph,
    jvk::Surface& surface,
    cvk::GraphicsPipeline const& pipeline,
    Cube const& cube,
    timgui::Imgui& imgui,
    std::optional<Targets>& targetsOpt
) {
    bool uploaded = false;
    glm::quat rotation{1.f, 0.f, 0.f, 0.f};
    auto last = std::chrono::steady_clock::now();

    FrameTimes frameTimes{};
    std::optional<cth::chrono::time_point_t> lastIterationStartOpt{};
    bool frameTimeCollapsed = false;

    while(!window.shouldClose()) {
        auto const iterationStart = cth::chrono::clock_t::now();
        if(lastIterationStartOpt.has_value())
            frameTimes.add(iterationStart - *lastIterationStartOpt);
        lastIterationStartOpt = iterationStart;

        window.pollEvents();
        auto imguiFrame = imgui.frame();
        frame_time_window(frameTimes, frameTimeCollapsed);
        auto& drawData = imguiFrame.render();

        auto& stage = (co_await graph.nextFrame()).stage(CUBE_STAGE_ID);

        auto const image = co_await surface.acquire(
            stage,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
        );
        // extent 0: no image
        if(!image) {
            stage.skip();
            co_await cth::co::this_coro::wait(IDLE_WAIT);
            continue;
        }

        // a new extent: the frame keeps the old targets until the gpu finished them
        if(!targetsOpt.has_value() || targetsOpt->color.extent() != image->extent) {
            if(targetsOpt.has_value()) {
                stage.retain(std::move(targetsOpt->colorView));
                stage.retain(std::move(targetsOpt->depthView));
                stage.retain(std::move(targetsOpt->color));
                stage.retain(std::move(targetsOpt->depth));
            }
            targetsOpt.emplace(core, image->extent);
        }

        auto const now = std::chrono::steady_clock::now();
        float const dt = std::chrono::duration<float>(now - last).count();
        last = now;
        rotation = glm::normalize(glm::angleAxis(dt * SPEED, AXIS) * rotation);

        auto const extent = image->extent;
        auto const model = glm::mat4_cast(rotation);
        auto const view = glm::lookAt(
            glm::vec3{0.f, 0.f, 3.f},
            glm::vec3{0.f},
            glm::vec3{0.f, 1.f, 0.f}
        );
        auto projection = glm::perspective(
            glm::radians(60.f),
            static_cast<float>(extent.width) / static_cast<float>(extent.height),
            0.1f,
            100.f
        );
        // vulkan: y of the clip space points down
        projection[1][1] *= -1.f;

        Draw const draw{
            .vertices = cube.vertices.address(),
            .mvp = projection * view * model,
            .normal = glm::mat3_cast(rotation),
            .texture = cube.sampled.idx(),
            .sampler = cube.sampler.idx(),
        };

        auto const submit = stage.begin();
        if(!uploaded) {
            cube.upload(stage, *submit);
            uploaded = true;
        }
        imgui.upload(stage, *submit, drawData);
        submit->use(cube.sampled, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
        submit->use(
            cube.vertices,
            {VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT}
        );
        // before the rendering: no barrier inside it, the bind inside it then needs none
        submit->use(
            cube.indices,
            {VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT}
        );

        auto const rendering = submit->beginRendering(
            {
                .colorAttachments = {
                    {
                        .view = targetsOpt->colorView,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        .clearValue = {.color = {.float32 = {0.02f, 0.02f, 0.03f, 1.f}}},
                        .resolveViewPtr = &*image->view,
                        .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
                    }
                },
                .depthAttachment = cvk::RenderingAttachment{
                    .view = targetsOpt->depthView,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .clearValue = {.depthStencil = {.depth = 1.f, .stencil = 0}},
                },
            }
        );
        submit->bind(pipeline);
        submit->setViewport(extent);
        submit->setScissor(extent);
        submit->bindIndices(cube.indices);
        submit->pushData(draw);
        submit->drawIndexed(36);

        // the rendering of the cube: the formats and samples of the targets
        imgui.draw(stage, *submit, drawData);
    }
}

/**
 * creates the window, the vulkan objects, the cube and the pipeline, then draws until the window closes
 * @details the declaration order is the destruction order backwards:
 * - the cube, the pipeline, the ImGui and the targets live until `~StageGraph()` waited for the gpu
 * - the surface is destroyed after the graph: the stages present to it
 */
void run() {
    cvk::Cvk const cvk{};
    cgfw::Window window{{.title = "cube", .extent = WINDOW_EXTENT}, cgfw::create};

    // the surface extensions of the window and the instance extensions of a present queue
    auto extensions = cgfw::instance_extensions();
    for(auto const extension : cvk::constants::PRESENT_INSTANCE_EXTENSIONS)
        if(!std::ranges::contains(extensions, extension))
            extensions.emplace_back(extension);
    cvk::Instance const instance{
        {.appName = "cube", .extensions = std::move(extensions)},
        cvk::create
    };

    // the core picks a queue that presents to the surface, the jvk surface takes it
    // scalar block layout: the vertices and the push data of cube.glsl
    auto windowSurface = window.surface(instance);
    jvk::Core core{
        instance,
        {.vk = {.queueProperties = {GRAPHICS | PRESENT}, .features = {.scalarBlockLayout = true}}},
        {windowSurface}
    };
    cvk::Core const& vk = core;
    auto const& limits = vk.physicalDevice().limits();
    if((limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts & MSAA_SAMPLES)
        == 0)
        throw std::runtime_error{
            std::format("the device offers no {}x msaa for color and depth", +MSAA_SAMPLES)
        };

    Cube const cube{core};

    cvk::Shader const vertex{core, VK_SHADER_STAGE_VERTEX_BIT, SHADER_DIR / "cube.vert.spv"};
    cvk::Shader const fragment{core, VK_SHADER_STAGE_FRAGMENT_BIT, SHADER_DIR / "cube.frag.spv"};
    cvk::GraphicsPipeline::Config pipelineConfig{
        .colorFormats = {SURFACE_FORMAT.format},
        .depthFormat = DEPTH_FORMAT
    };
    pipelineConfig.multisample.rasterizationSamples = MSAA_SAMPLES;
    // counter-clockwise seen from outside, the flipped y of the projection keeps the orientation on the screen
    pipelineConfig.rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineConfig.addStage(vertex).addStage(fragment);
    pipelineConfig.descriptorMappings = core.resourceHeap().mappings();
    pipelineConfig.descriptorMappings.append_range(core.samplerHeap().mappings());
    cvk::GraphicsPipeline const pipeline{core, std::move(pipelineConfig), cvk::create};

    timgui::Imgui imgui{
        core,
        window,
        {.colorFormat = SURFACE_FORMAT.format, .depthFormat = DEPTH_FORMAT, .samples = MSAA_SAMPLES}
    };

    std::optional<Targets> targetsOpt{};
    jvk::Surface surface{
        core,
        core.queue(QUEUE_IDX),
        {.swapchain = {.formats = {SURFACE_FORMAT}}},
        std::move(windowSurface),
    };
    jvk::StageGraph graph{core, {.stages = {{.queueIdx = QUEUE_IDX}}}};

    core.executor().sync_wait(
        draw_frames(window, core, graph, surface, pipeline, cube, imgui, targetsOpt)
    );
}
}

int main() {
    try { run(); }
    catch(std::exception const& e) {
        std::println(stderr, "cube failed: {}", e.what());
        return 1;
    }
}
