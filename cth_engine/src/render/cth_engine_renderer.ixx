module;
#include <cth/io/io_log.hpp>

export module cth.engine.renderer;

import cth.engine.render.stage_config;
import cth.engine.render.stage;
import cth.engine.render.dag;

import cth.vk.base.core;
import cth.vk.render.sync.pipeline_wait_stage;
import cth.engine.render.stage_config;
import cth.engine.render.dag;
import cth.vk.render.sync.pulse;
import cth.vk.constants;
import cth.vk.render.sync.semaphore;

import cth.ptr.not_null;
import cth.io.log;

import std;

export namespace cth::vk {
struct RendererConfig {
    using StageConfig = RenderStageConfig;
    using id_t = size_t;
    using dependencies_t = dag<id_t, PipelineWaitStage::stage_t>;
    using stage_map_t = std::map<id_t, StageConfig>;

    stage_map_t stages;
    dependencies_t stageDependencies;

    void removeUnusedDependencies();
    static void debugCheck(RendererConfig const&);
};



}

export namespace cth::vk {
class Renderer {
public:
    using Config = RendererConfig;
    using id_t = Config::id_t;
    using StageConfig = Config::StageConfig;

    static constexpr auto GROUP_SIZE = StageConfig::GROUP_SIZE;
    /**
     * @brief begin and end semaphores for a frame
     */
    static constexpr auto FRAME_SEMAPHORES = 2 * GROUP_SIZE;

    static constexpr size_t MAX_SECONDARY_CMD_BUFFERS = 0;

private:
    static constexpr VkPipelineStageFlags FRAME_BEGIN_STAGE = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

public:
    /**
     * @brief creates
     * @param config to use
     */
    Renderer(Core const& core, RenderPulse const& pulse, Config config);
    /**
     * @brief constructs and creates
     * @param core requires @ref Core::created()
     * @param config to use
     * @details calls: Renderer::Render(Core const&, Config)
     */
    Renderer(Core const& core, RenderPulse const& pulse, Config const& config, create_t);

    ~Renderer();

    /**
     * @brief creates the renderer
     * @attention requires @ref Core::created()
     */
    std::map<id_t, RenderStage*> create();

    /**
     * @brief destroys the renderer
     * @attention requires @ref created()
     */
    void destroy();

    /**
     * @brief destroys if @ref created()
     */
    void optDestroy() { if(created()) destroy(); }

private:
    void initDependencySemaphores(size_t edges);

    static void linkStageDependencies(Config::stage_map_t& stages, Config::dependencies_t const& dag, id_t source_id,
        std::span<Semaphore*> stage_semaphores);


    void linkDependencies(Config::stage_map_t& stages, Config::dependencies_t const& dag);

    void initRenderStages(Config::stage_map_t const& stage_configs);


    void createSemaphores();
    void createStages();

    bool _created = false;

    cth::not_null<Core const*> _core;
    cth::not_null<RenderPulse const*> _pulse;

    std::vector<Semaphore> _stageSemaphores;
    std::map<id_t, RenderStage> _renderStages;

public:
    [[nodiscard]] bool created() const { return _created; }
    [[nodiscard]] RenderStage& stage(id_t id);
    [[nodiscard]] std::map<id_t, RenderStage*> stages();
    [[nodiscard]] RenderPulse const& pulse() const { return *_pulse; }

    static void debugCheck(Renderer const&);

    Renderer(Renderer const& other) = delete;
    Renderer& operator=(Renderer const& other) = delete;
    Renderer(Renderer&& other) noexcept = default;
    Renderer& operator=(Renderer&& other) noexcept = default;
};



}

namespace cth::vk {
inline void RendererConfig::debugCheck(RendererConfig const& config) {
    CTH_CRITICAL(config.stageDependencies.cyclic(), "stage dependency dag invalid, cyclic nodes: {}", config.stageDependencies.cyclics()) {}


    CTH_CRITICAL(
        std::ranges::any_of(
            config.stages | std::views::keys,
            [&config](RendererConfig::id_t const id) { return !config.stageDependencies.contains(id); }
        ),
        "the dependency graph must contain all config id's"
    ) {
        auto view = config.stages
            | std::views::keys
            | std::views::filter([&dag = config.stageDependencies](auto const id) { return !dag.contains(id); });

        details->add("missing id's: {}", view);
    }
}
}

namespace cth::vk {
inline void Renderer::debugCheck(Renderer const& renderer) {
    CTH_CRITICAL(!renderer.created(), "renderer must be created"){}
}
}
