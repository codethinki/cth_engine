#pragma once
#include "jvk/data/heap_idx.hpp"
#include "jvk/data/resource_heap.hpp"

#include <cth/ptr/ref_wrapper.hpp>
#include <imgui.h>
#include <volk.h>

#include <concepts>
#include <filesystem>
#include <memory>


namespace cgfw {
class Window;
}

namespace jvk {
class Core;
class Stage;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;
}

namespace timgui {
namespace dev {
    struct ImguiState;

    /**
     * command buffer that `Imgui` records into
     * @details the .cpp of `Imgui` instantiates `upload()` and `draw()` for these two types
     */
    template<class T>
    concept cmd_buffer = std::same_as<T, jvk::PrimaryCmdBuffer>
        || std::same_as<T, jvk::SecondaryCmdBuffer>;
}

/**
 * ImGui on a cgfw window, drawn with a jvk core
 * @details
 * - owns: the ImGui context, the ImPlot context, the pipeline, the ImGui textures (font atlas)
 * - one object per window: input from the window, draws for its renderings
 * - ImGui and ImPlot calls only inside a `Frame` of this object
 * @attention one `Frame` at a time: ImGui and ImPlot keep the current context in a global
 */
class Imgui {
public:
    struct Config;
    class Frame;

    /**
     * creates the ImGui context, the ImPlot context and the pipeline
     * @details makes the contexts current
     * @param window input and extent of the frames, outlives this object
     * @pre the core has a resource heap and a sampler heap, see `jvk::HeapsConfig`
     * @throws cvk::vk_result_exception result of the shader, pipeline or sampler creation
     * @throws jvk::jvk_exception reason: every cell of the sampler heap is in use
     */
    Imgui(jvk::Core const&, cgfw::Window const& window, Config const&);

    /**
     * destroys the ImPlot context, the ImGui context, the pipeline and the textures
     * @pre the gpu finished every frame that `draw()` recorded into, e.g. `~StageGraph()` waited
     */
    ~Imgui();

    /**
     * starts an ImGui frame
     * @details
     * - makes the ImGui and the ImPlot context of this object current
     * - passes the events of the last `cgfw::Window::pollEvents()`
     * - passes the extent and the time since the last frame
     * - calls `ImGui::NewFrame()`
     * @pre no `Frame` of any `Imgui` exists
     */
    [[nodiscard]] Frame frame();

    /**
     * records the texture requests of the draw data: create, update, destroy
     * @details
     * - copies through a staging buffer, `stage` retains it until the gpu finished
     * - a destroyed texture goes to `stage` too
     * - an update copies the whole texture
     * - leaves the textures ready to sample in the fragment shader
     * - `draw()` needs no barrier in its rendering
     * @param stage of `cmd`
     * @param cmd outside a rendering, before the `draw()` of `drawData`
     * @param[out] drawData its textures get `ImTextureStatus_OK` or `ImTextureStatus_Destroyed`
     */
    void upload(jvk::Stage& stage, dev::cmd_buffer auto& cmd, ImDrawData& drawData);

    /**
     * records the draws of the draw data
     * @details
     * - writes the vertices and indices into host buffers
     * - `stage` retains the buffers until the gpu finished
     * - binds the pipeline, sets the viewport and a scissor per draw command
     * - `TotalVtxCount == 0` or an empty framebuffer -> records nothing
     * @param stage of `cmd`
     * @param cmd inside a rendering with the formats and samples of `Config`
     * @param drawData of `Frame::render()`
     * @pre `upload()` of `drawData` before
     */
    void draw(jvk::Stage& stage, dev::cmd_buffer auto& cmd, ImDrawData const& drawData) const;

    /**
     * ImGui texture of an image of the resource heap, for `ImGui::Image()`
     * @details the shader samples it with the sampler of this object
     */
    [[nodiscard]] ImTextureRef texture(jvk::heap_idx<jvk::SampledImage> imageIdx) const;

private:
    std::unique_ptr<dev::ImguiState> _state;

public:
    Imgui(Imgui const&) = delete;
    Imgui& operator=(Imgui const&) = delete;
    Imgui(Imgui&&) noexcept;
    Imgui& operator=(Imgui&&) noexcept;
};
}

namespace timgui {
struct Imgui::Config {
    /**
     * color attachment of the renderings of `draw()`
     * @details sRGB format -> the shader converts the colors of ImGui to linear
     */
    VkFormat colorFormat;
    /** depth attachment of those renderings, `VK_FORMAT_UNDEFINED` -> none */
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    /** stencil attachment of those renderings, `VK_FORMAT_UNDEFINED` -> none */
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT; ///< samples of those renderings
    std::filesystem::path iniFile{}; ///< ImGui settings file, empty -> no file
};
}

namespace timgui {
/**
 * scope of an ImGui frame
 * @details the destructor ends a frame without `render()`, no draw data (e.g. on an exception)
 */
class Imgui::Frame {
public:
    /**
     * starts the frame, see `Imgui::frame()`
     * @pre no `Frame` of any `Imgui` exists
     */
    explicit Frame(Imgui& imgui);
    ~Frame();

    /**
     * ends the frame and builds the draw data
     * @return valid until the next `Imgui::frame()`
     * @pre `render()` was not called
     */
    [[nodiscard]] ImDrawData& render();

private:
    cth::ref_wrapper<Imgui> _imgui;
    bool _rendered = false;

public:
    Frame(Frame const&) = delete;
    Frame& operator=(Frame const&) = delete;
    Frame(Frame&&) = delete;
    Frame& operator=(Frame&&) = delete;
};
}
