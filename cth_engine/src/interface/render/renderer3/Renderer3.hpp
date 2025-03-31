#pragma once
#include "dag.hpp"

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
struct Renderer3StageConfig {
    static constexpr auto GROUP_SIZE = constants::FRAMES_IN_FLIGHT;
    using id_t = size_t;

    id_t uniqueID;

    cth::not_null<Queue const*> queue;
    bool parallel = false;
    std::vector<Semaphore*> signalSemaphores{};
    std::vector<PipelineWaitStage> waitStages{};
};

struct Renderer3Config {
    using StageConfig = Renderer3StageConfig;
    using id_t = size_t;
    using dependencies_t = dag<id_t, PipelineWaitStage::stage_t>;

    std::vector<StageConfig> stages;
    dependencies_t stageDependencies;

    void removeUnusedDependencies();
    static void debugCheck(Renderer3Config const&);
};



}

namespace cth::vk {

class Renderer3 {
public:
    using Config = Renderer3Config;
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
    Renderer3(Core const& core, Config config);

    void create();

    void destroy();
    void optDestroy() { if(created()) destroy(); }

private:
    void initFrameSemaphores();
    void initDependencySemaphores(size_t edges);

    using stage_config_map_t = std::map<StageConfig::id_t, StageConfig>;
    static void linkStageDependencies(stage_config_map_t& stages, Config::dependencies_t const& dag,
        StageConfig& source_stage, std::span<Semaphore*> stage_semaphores);

    void linkRoot(StageConfig& config);
    void linkDestination(StageConfig& config);

    void linkFrameBounds(stage_config_map_t& stages, Config::dependencies_t const& dag);
    void linkDependencies(stage_config_map_t& stages, Config::dependencies_t const& dag);
    void linkStages(std::span<StageConfig> stages, Config::dependencies_t const& dag);

    std::map<uint32_t, size_t> initSharedCmdPools(std::span<StageConfig const> stages);
    size_t initUniqueCmdPool(uint32_t queue_family_index);

    std::vector<size_t> initStagePools(std::span<StageConfig const> stages);

    void initRenderStages(std::vector<StageConfig> stage_configs, std::span<size_t const> pool_indices);


    void createSemaphores();
    void createCmdPools();
    void createStages();

    cth::not_null<Core const*> _core;

    std::vector<Semaphore> _frameSemaphores;
    std::vector<Semaphore> _stageSemaphores;
    std::vector<CmdPool> _stageCmdPools;
    std::vector<RenderStage> _renderStages;

public:
    [[nodiscard]] bool created() const;

    static void debugCheck(Renderer3 const&);
};


}

namespace cth::vk {
inline void Renderer3Config::debugCheck(Renderer3Config const& config) {
    CTH_CRITICAL(!config.stageDependencies.valid(), "stage dependency dag invalid, cyclic nodes: {}", config.stageDependencies.cyclics()) {}
    CTH_CRITICAL(
        std::ranges::any_of(config.stages, [&config](auto const& stageConfig) {
            return !config.stageDependencies.contains(stageConfig.uniqueID);
            }), "the dependency graph must contain all config id's") {
        auto const view = config.stages
            | std::views::transform([](auto const& stage_config) { return stage_config.uniqueID; })
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
