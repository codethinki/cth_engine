#include "timgui/imgui.hpp"

#include "input.hpp"
#include "shaders/imgui_frag.hpp"
#include "shaders/imgui_vert.hpp"

#include "jvk/base/core.hpp"
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/data/buffer.hpp"
#include "jvk/data/image.hpp"
#include "jvk/data/sampler_heap.hpp"
#include "jvk/graph/stage.hpp"

#include "cgfw/window/window.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/buffer/device_ptr.hpp"
#include "cvk/data/descriptor/image_descriptor.hpp"
#include "cvk/data/img/image_config.hpp"
#include "cvk/data/img/sampler_config.hpp"
#include "cvk/render/pipeline/graphics_pipeline.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"

#include <imgui.h>
#include <implot.h>
#include <vk_mem_alloc.h>
#include <volk.h>
#include <vulkan/utility/vk_format_utils.h>

#include <cth/chrono.hpp>
#include <cth/data/hash_map.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>


namespace timgui::dev {
namespace {
    // one `Frame` in the program: ImGui keeps the current context in a global
    bool frameExists = false;
}

/**
 * push data of the draws, `Draw` of imgui.glsl in std430 layout
 */
struct Draw {
    cvk::device_ptr<ImDrawVert> vertices;
    ImVec2 scale; ///< display to clip space
    ImVec2 translate; ///< display to clip space
    jvk::heap_idx<jvk::SampledImage> texture;
    jvk::heap_idx<jvk::Sampler> sampler;
};
static_assert(offsetof(Draw, scale) == 8 && offsetof(Draw, translate) == 16);
static_assert(offsetof(Draw, texture) == 24 && offsetof(Draw, sampler) == 28);
static_assert(sizeof(Draw) == 32, "the push data must match `Draw` of imgui.glsl");

static_assert(sizeof(ImDrawVert) == 20, "the vertices must match `Vertex` of imgui.glsl");
static_assert(offsetof(ImDrawVert, uv) == 8 && offsetof(ImDrawVert, col) == 16);

/**
 * ImGui texture on the gpu
 * @details the heap cell refers to the image: the texture does not move
 */
struct Texture {
    /**
     * creates the image with the size of the texture data and its heap cell
     */
    Texture(jvk::Core const& core, ImTextureData const& data);

    jvk::Image image;
    jvk::SampledImage sampled;
};

/**
 * deleter of the ImGui and the ImPlot context
 */
struct context_deleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
    void operator()(ImPlotContext* context) const { ImPlot::DestroyContext(context); }
};

/**
 * state of an `Imgui`
 * @details the members of the ImGui context come before it: it uses them up to its destruction
 */
struct ImguiState {
    /**
     * creates the ImGui context, the ImPlot context, the pipeline and the sampler
     * @details makes the contexts current
     */
    ImguiState(jvk::Core const& core, cgfw::Window const& window, Imgui::Config const& config);

    cth::ref_wrapper<jvk::Core const> core;
    cth::ref_wrapper<cgfw::Window const> window;

    /**
     * `Imgui::Config::iniFile` in UTF-8, `ImGuiIO::IniFilename` points into it
     * @details the destruction of the context saves the settings to it
     */
    std::string iniFile;

    std::unique_ptr<ImGuiContext, context_deleter> context;

    /** after `context`: destroyed before it */
    std::unique_ptr<ImPlotContext, context_deleter> plotContext{};

    cvk::GraphicsPipeline pipeline;
    jvk::Sampler sampler;

    /**
     * the textures that the gpu has, per ImGui texture
     * @details `std::unique_ptr`: a rehash moves the values, the heap cell refers to the image
     */
    cth::dt::flat_hash_map<ImTextureData const*, std::unique_ptr<Texture>> textures{};

    modifier_keys modifiers{};

    std::optional<cth::chrono::time_point_t> lastFrameOpt{}; ///< `std::nullopt` -> no frame yet
};
}

namespace timgui {
namespace {
    constexpr float FIRST_DELTA_TIME = 1.f / 60.f; // seconds, ImGui needs a positive time

    constexpr cvk::ImageConfig TEXTURE_CONFIG{
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    constexpr cvk::BaseBuffer::Config STAGING_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
    };

    constexpr cvk::BaseBuffer::Config VERTEX_CONFIG{
        .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
    };

    constexpr cvk::BaseBuffer::Config INDEX_CONFIG{
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .memory = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
    };

    constexpr cvk::SamplerConfig SAMPLER_CONFIG{
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxAnisotropy = 0.f,
    };

    // the alpha of the attachment gets the coverage of the draws
    constexpr VkPipelineColorBlendAttachmentState BLENDING{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT,
    };

    constexpr auto TEXTURE_OFFSET = static_cast<uint32_t>(offsetof(dev::Draw, texture));

    void make_current(dev::ImguiState const& state) {
        ImGui::SetCurrentContext(state.context.get());
        ImPlot::SetCurrentContext(state.plotContext.get());
    }

    // `+ 1`: `ImTextureID_Invalid` is `0`, the heap index `0` is a valid cell
    [[nodiscard]] constexpr ImTextureID to_texture_id(jvk::heap_idx<jvk::SampledImage> idx) {
        return ImTextureID{idx.get()} + 1;
    }

    [[nodiscard]] constexpr jvk::heap_idx<jvk::SampledImage> to_heap_idx(ImTextureID id) {
        CTH_CRITICAL(id == ImTextureID_Invalid, "the texture id must be valid") {}
        return jvk::heap_idx<jvk::SampledImage>{static_cast<uint32_t>(id - 1)};
    }

    [[nodiscard]] constexpr std::optional<VkRect2D> to_scissor(
        ImVec4 const& clipRect,
        ImVec2 offset,
        ImVec2 scale,
        VkExtent2D extent
    ) {
        auto const width = static_cast<float>(extent.width);
        auto const height = static_cast<float>(extent.height);
        auto const minX = std::max((clipRect.x - offset.x) * scale.x, 0.f);
        auto const minY = std::max((clipRect.y - offset.y) * scale.y, 0.f);
        auto const maxX = std::min((clipRect.z - offset.x) * scale.x, width);
        auto const maxY = std::min((clipRect.w - offset.y) * scale.y, height);
        if(maxX <= minX || maxY <= minY)
            return std::nullopt;

        return VkRect2D{
            .offset = {static_cast<int32_t>(minX), static_cast<int32_t>(minY)},
            .extent = {static_cast<uint32_t>(maxX - minX), static_cast<uint32_t>(maxY - minY)},
        };
    }

    // the config of the pipeline keeps the destroyed shaders: no second `create()`
    [[nodiscard]] cvk::GraphicsPipeline create_pipeline(
        jvk::Core const& core,
        Imgui::Config const& config
    ) {
        cvk::Shader const vertex{core, VK_SHADER_STAGE_VERTEX_BIT, dev::shaders::IMGUI_VERT};
        cvk::Shader const fragment{core, VK_SHADER_STAGE_FRAGMENT_BIT, dev::shaders::IMGUI_FRAG};

        // `SRGB_TARGET` of imgui.vert
        VkBool32 const srgbTarget = vkuFormatIsSRGB(config.colorFormat) ? VK_TRUE : VK_FALSE;
        VkSpecializationMapEntry const srgbEntry{
            .constantID = 0,
            .offset = 0,
            .size = sizeof(srgbTarget),
        };
        VkSpecializationInfo const specialization{
            .mapEntryCount = 1,
            .pMapEntries = &srgbEntry,
            .dataSize = sizeof(srgbTarget),
            .pData = &srgbTarget,
        };

        cvk::GraphicsPipeline::Config pipelineConfig{
            .colorBlendAttachments = {BLENDING},
            .colorFormats = {config.colorFormat},
            .depthFormat = config.depthFormat,
            .stencilFormat = config.stencilFormat,
        };
        pipelineConfig.rasterization.cullMode = VK_CULL_MODE_NONE;
        pipelineConfig.multisample.rasterizationSamples = config.samples;
        pipelineConfig.depthStencil.depthTestEnable = VK_FALSE;
        pipelineConfig.depthStencil.depthWriteEnable = VK_FALSE;
        pipelineConfig.addStage(vertex, &specialization).addStage(fragment);
        pipelineConfig.descriptorMappings = core.resourceHeap().mappings();
        pipelineConfig.descriptorMappings.append_range(core.samplerHeap().mappings());

        return cvk::GraphicsPipeline{core, std::move(pipelineConfig), cvk::create};
    }

    void copy_pixels(
        jvk::Stage& stage,
        dev::cmd_buffer auto& cmd,
        dev::Texture const& texture,
        ImTextureData const& data
    ) {
        std::span<uint8_t const> const pixels{
            data.Pixels,
            static_cast<size_t>(data.GetSizeInBytes())
        };
        jvk::Buffer<uint8_t> staging{cmd.core(), STAGING_CONFIG, pixels.size()};
        staging.write(pixels);

        texture.image.copy(cmd, staging);
        cmd.use(texture.sampled, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

        stage.retain(std::move(staging));
    }

    template<class T>
    void write_lists(
        cvk::Buffer<T> const& buffer,
        ImVector<ImDrawList*> const& lists,
        ImVector<T> ImDrawList::* member
    ) {
        auto const mapping = buffer.map();
        auto out = mapping.begin();
        for(ImDrawList const* list : lists)
            out = std::ranges::copy(list->*member, out).out;
        buffer.flush();
    }
}
}

namespace timgui::dev {
Texture::Texture(jvk::Core const& core, ImTextureData const& data) :
    image{
        core,
        TEXTURE_CONFIG,
        VkExtent2D{static_cast<uint32_t>(data.Width), static_cast<uint32_t>(data.Height)}
    },
    sampled{core.resourceHeap().insert(cvk::SampledImageDescriptor{image})} {}

ImguiState::ImguiState(
    jvk::Core const& core,
    cgfw::Window const& window,
    Imgui::Config const& config
) :
    core{core},
    window{window},
    iniFile{std::from_range, config.iniFile.u8string()},
    context{ImGui::CreateContext()},
    pipeline{create_pipeline(core, config)},
    sampler{core.samplerHeap().insert(SAMPLER_CONFIG)} {
    // ImPlot allocates through the current ImGui context
    ImGui::SetCurrentContext(context.get());
    plotContext.reset(ImPlot::CreateContext());
    make_current(*this);

    auto& io = ImGui::GetIO();
    io.BackendPlatformName = "timgui_cgfw";
    io.BackendRendererName = "timgui_jvk";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures
        | ImGuiBackendFlags_RendererHasVtxOffset;
    io.IniFilename = iniFile.empty() ? nullptr : iniFile.c_str();
    io.Fonts->TexDesiredFormat = ImTextureFormat_RGBA32;
}
}

namespace timgui {
Imgui::Imgui(jvk::Core const& core, cgfw::Window const& window, Config const& config) :
    _state{std::make_unique<dev::ImguiState>(core, window, config)} {}

Imgui::~Imgui() = default;

Imgui::Frame Imgui::frame() { return Frame{*this}; }

void Imgui::upload(jvk::Stage& stage, dev::cmd_buffer auto& cmd, ImDrawData& drawData) {
    CTH_CRITICAL(cmd.insideRendering(), "the command buffer must be outside a rendering") {}
    if(drawData.Textures == nullptr)
        return;

    auto& state = *_state;
    for(ImTextureData* dataPtr : *drawData.Textures) {
        auto& data = *dataPtr;
        switch(data.Status) {
            case ImTextureStatus_WantCreate: {
                CTH_CRITICAL(
                    data.Format != ImTextureFormat_RGBA32,
                    "the texture must be RGBA32, got {}",
                    static_cast<int>(data.Format)
                ) {}
                CTH_CRITICAL(state.textures.contains(&data), "the texture must be new") {}

                auto texture = std::make_unique<dev::Texture>(state.core, data);
                copy_pixels(stage, cmd, *texture, data);
                data.SetTexID(to_texture_id(texture->sampled.idx()));
                state.textures.emplace(&data, std::move(texture));
                data.SetStatus(ImTextureStatus_OK);
                break;
            }
            case ImTextureStatus_WantUpdates: {
                auto const textureIt = state.textures.find(&data);
                CTH_CRITICAL(textureIt == state.textures.end(), "the texture must exist") {}

                copy_pixels(stage, cmd, *textureIt->second, data);
                data.SetStatus(ImTextureStatus_OK);
                break;
            }
            case ImTextureStatus_WantDestroy: {
                auto const textureIt = state.textures.find(&data);
                CTH_CRITICAL(textureIt == state.textures.end(), "the texture must exist") {}

                // frames in flight may still sample it: the stage keeps the heap cell and the image
                auto const texture = std::move(textureIt->second);
                state.textures.erase(textureIt);
                stage.retain(std::move(texture->sampled));
                stage.retain(std::move(texture->image));

                data.SetTexID(ImTextureID_Invalid);
                data.SetStatus(ImTextureStatus_Destroyed);
                break;
            }
            default: break;
        }
    }
}

void Imgui::draw(jvk::Stage& stage, dev::cmd_buffer auto& cmd, ImDrawData const& drawData) const {
    CTH_CRITICAL(!cmd.insideRendering(), "the command buffer must be inside a rendering") {}

    VkExtent2D const extent{
        static_cast<uint32_t>(drawData.DisplaySize.x * drawData.FramebufferScale.x),
        static_cast<uint32_t>(drawData.DisplaySize.y * drawData.FramebufferScale.y),
    };
    if(drawData.TotalVtxCount == 0 || extent.width == 0 || extent.height == 0)
        return;

    auto const& state = *_state;
    cvk::Core const& vk = *state.core;

    auto const vertexCount = static_cast<size_t>(drawData.TotalVtxCount);
    auto const indexCount = static_cast<size_t>(drawData.TotalIdxCount);
    jvk::Buffer<ImDrawVert> vertices{vk, VERTEX_CONFIG, vertexCount};
    jvk::Buffer<ImDrawIdx> indices{vk, INDEX_CONFIG, indexCount};
    write_lists(vertices, drawData.CmdLists, &ImDrawList::VtxBuffer);
    write_lists(indices, drawData.CmdLists, &ImDrawList::IdxBuffer);

    ImVec2 const scale{2.f / drawData.DisplaySize.x, 2.f / drawData.DisplaySize.y};
    dev::Draw const push{
        .vertices = vertices.address(),
        .scale = scale,
        .translate = {
            -1.f - drawData.DisplayPos.x * scale.x,
            -1.f - drawData.DisplayPos.y * scale.y
        },
        .texture = {},
        .sampler = state.sampler.idx(),
    };

    // the render state: at the start and for `ImDrawCallback_ResetRenderState`
    auto const bindState = [&] {
        cmd.bind(state.pipeline);
        cmd.bindIndices(indices);
        cmd.setViewport(extent);
        cmd.pushData(push);
    };
    bindState();

    // one buffer for all lists: the offsets of the list in it
    uint32_t firstIndex = 0;
    int32_t firstVertex = 0;
    for(ImDrawList const* list : drawData.CmdLists) {
        for(ImDrawCmd const& drawCmd : list->CmdBuffer) {
            if(drawCmd.UserCallback != nullptr) {
                if(drawCmd.UserCallback == ImDrawCallback_ResetRenderState)
                    bindState();
                else
                    drawCmd.UserCallback(list, &drawCmd);
                continue;
            }

            auto const scissorOpt = to_scissor(
                drawCmd.ClipRect,
                drawData.DisplayPos,
                drawData.FramebufferScale,
                extent
            );
            if(!scissorOpt)
                continue;

            cmd.setScissor(*scissorOpt);
            cmd.pushData(to_heap_idx(drawCmd.GetTexID()), TEXTURE_OFFSET);
            cmd.drawIndexed(
                drawCmd.ElemCount,
                1,
                firstIndex + drawCmd.IdxOffset,
                firstVertex + static_cast<int32_t>(drawCmd.VtxOffset)
            );
        }
        firstIndex += static_cast<uint32_t>(list->IdxBuffer.Size);
        firstVertex += list->VtxBuffer.Size;
    }

    stage.retain(std::move(vertices));
    stage.retain(std::move(indices));
}

ImTextureRef Imgui::texture(jvk::heap_idx<jvk::SampledImage> imageIdx) const {
    return ImTextureRef{to_texture_id(imageIdx)};
}

Imgui::Imgui(Imgui&&) noexcept = default;
Imgui& Imgui::operator=(Imgui&&) noexcept = default;

template void Imgui::upload(jvk::Stage&, jvk::PrimaryCmdBuffer&, ImDrawData&);
template void Imgui::upload(jvk::Stage&, jvk::SecondaryCmdBuffer&, ImDrawData&);
template void Imgui::draw(jvk::Stage&, jvk::PrimaryCmdBuffer&, ImDrawData const&) const;
template void Imgui::draw(jvk::Stage&, jvk::SecondaryCmdBuffer&, ImDrawData const&) const;
}

namespace timgui {
Imgui::Frame::Frame(Imgui& imgui) : _imgui{imgui} {
    CTH_CRITICAL(dev::frameExists, "no frame of any imgui may exist") {}

    auto& state = *imgui._state;
    make_current(state);
    auto& io = ImGui::GetIO();

    auto const extent = state.window->extent();
    io.DisplaySize = ImVec2{static_cast<float>(extent.width), static_cast<float>(extent.height)};

    auto const now = cth::chrono::clock_t::now();
    io.DeltaTime = state.lastFrameOpt.has_value()
        ? std::chrono::duration<float>(now - *state.lastFrameOpt).count()
        : FIRST_DELTA_TIME;
    state.lastFrameOpt = now;

    for(auto const& event : state.window->events())
        dev::add_event(io, event, state.modifiers);

    ImGui::NewFrame();
    dev::frameExists = true;
}

Imgui::Frame::~Frame() {
    if(!_rendered) {
        make_current(*_imgui->_state);
        ImGui::EndFrame();
    }
    dev::frameExists = false;
}

ImDrawData& Imgui::Frame::render() {
    CTH_CRITICAL(_rendered, "render() must not be called twice") {}

    make_current(*_imgui->_state);
    ImGui::Render();
    _rendered = true;
    return *ImGui::GetDrawData();
}
}
