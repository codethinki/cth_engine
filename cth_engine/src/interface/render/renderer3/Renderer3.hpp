#pragma once
#include "dag.hpp"
#include "RenderStageConfig.hpp"

#include "src/vulkan/render/control/CthPipelineWaitStage.hpp"

#include <map>

namespace cth::vk {
class RenderStage;
class CmdPool;
class Queue;
class TimelineSemaphore;
class Core;
class Semaphore;
}

namespace cth::vk {

struct Renderer3Config {
    using StageConfig = RenderStageConfig;
    using id_t = size_t;
    using dependencies_t = dag<id_t, PipelineWaitStage::stage_t>;
    using stage_map_t = std::map<id_t, StageConfig>;

    stage_map_t stages;
    dependencies_t stageDependencies;

    void removeUnusedDependencies();
    static void debugCheck(Renderer3Config const&);
};



}

namespace cth::vk {

class Renderer3 {
public:
    using Config = Renderer3Config;
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
    Renderer3(Core const& core, Config config);
    /**
     * @brief constructs and creates
     * @param core requires @ref Core::created()
     * @param config to use
     * @details calls: Renderer::Render(Core const&, Config)
     */
    Renderer3(Core const& core, Config const& config, create_t);

    /**
     * @brief creates the renderer
     * @attention requires @ref Core::created()
     */
    void create();

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
    void initFrameSemaphores();
    void initDependencySemaphores(size_t edges);

    static void linkStageDependencies(Config::stage_map_t& stages, Config::dependencies_t const& dag, id_t source_id,
        std::span<Semaphore*> stage_semaphores);

    void linkRoot(StageConfig& stage_config);
    void linkDestination(StageConfig& stage_config);

    void linkFrameBounds(Config::stage_map_t& stages, Config::dependencies_t const& dag);
    void linkDependencies(Config::stage_map_t& stages, Config::dependencies_t const& dag);
    void linkStages(Config::stage_map_t stages, Config::dependencies_t const& dag);

    void initRenderStages(Config::stage_map_t const& stage_configs);


    void createSemaphores();
    void createStages();

    cth::not_null<Core const*> _core;

    std::vector<Semaphore> _frameSemaphores;
    std::vector<Semaphore> _stageSemaphores;
    std::map<id_t, RenderStage> _renderStages;

public:
    [[nodiscard]] bool created() const;

    static void debugCheck(Renderer3 const&);
};


}

namespace cth::vk {
inline void Renderer3Config::debugCheck(Renderer3Config const& config) {
    CTH_CRITICAL(!config.stageDependencies.valid(), "stage dependency dag invalid, cyclic nodes: {}", config.stageDependencies.cyclics()) {}
    CTH_CRITICAL(
        std::ranges::any_of(
            config.stages | std::views::keys,
            [&config](Renderer3Config::id_t const id) { return !config.stageDependencies.contains(id); }
        ),
        "the dependency graph must contain all config id's"
    ) {
        auto const view = config.stages
            | std::views::keys
            | std::views::filter([&dag = config.stageDependencies](auto const id) { return !dag.contains(id); });

        details->add("missing id's: {}", view);
    }
}
}

namespace cth::vk {
inline void Renderer3::debugCheck(Renderer3 const& renderer) {
    CTH_CRITICAL(!renderer.created(), "renderer must be created"){}
}
}
