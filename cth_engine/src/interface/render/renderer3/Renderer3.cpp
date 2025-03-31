#include "Renderer3.hpp"

#include "RenderStage.hpp"

#include "src/vulkan/base/CthCore.hpp"
#include "src/vulkan/base/queue/CthQueue.hpp"
#include "src/vulkan/base/queue/CthSubmitInfo.hpp"
#include "src/vulkan/render/cmd/CthCmdPool.hpp"
#include "src/vulkan/render/control/CthSemaphore.hpp"
#include "src/vulkan/utility/cth_constants.hpp"

namespace cth::vk {
void Renderer3Config::removeUnusedDependencies() {
    auto unusedIds = stageDependencies.nodes();
    for(auto const& stage : stages) unusedIds.erase(stage.uniqueID);
    for(auto const& id : unusedIds) stageDependencies.erase(id);
}

}

namespace cth::vk {
Renderer3::Renderer3(Core const& core, Config config) : _core{&core} {
    Config::debugCheck(config);

    auto& [stages, stageDependencies] = config;
    config.removeUnusedDependencies();

    initFrameSemaphores();
    initDependencySemaphores(stageDependencies.edgeCount());

    linkStages(stages, stageDependencies);

    std::vector<size_t> const poolIndices = initStagePools(stages);
    initRenderStages(std::move(stages), poolIndices);
}
void Renderer3::create() {
    optDestroy();

    createSemaphores();
    createCmdPools();
    createStages();
}

void Renderer3::destroy() {
    for(auto& stage : _renderStages) stage.destroy();
    for(auto& pool : _stageCmdPools) pool.destroy();
    for(auto& semaphore : _stageSemaphores) semaphore.destroy();
    for(auto& semaphore : _frameSemaphores) semaphore.destroy();
}

void Renderer3::initFrameSemaphores() {
    for(size_t i = 0; i < 2 * GROUP_SIZE; i++)
        _frameSemaphores.emplace_back(*_core);
}
void Renderer3::initDependencySemaphores(size_t edges) {
    size_t const semaphores = edges * RenderStageConfig::GROUP_SIZE;
    _stageSemaphores.reserve(semaphores);

    for(size_t i = 0; i < semaphores; ++i) _stageSemaphores.emplace_back(*_core);

}

void Renderer3::linkStageDependencies(stage_config_map_t& stages, Config::dependencies_t const& dag,
    StageConfig& source_stage, std::span<Semaphore*> stage_semaphores) {

    size_t const sourceId = source_stage.uniqueID;
    auto const& dependencies = dag.at(sourceId);

    for(auto const [dependencyId, dependencySemaphores] : std::views::zip(dependencies, stage_semaphores | std::views::chunk(GROUP_SIZE))) {
        auto& targetStage = stages.at(dependencyId);

        targetStage.signalSemaphores.append_range(dependencySemaphores);
        for(auto semaphore : dependencySemaphores)
            source_stage.waitStages.emplace_back(dag.annotation(sourceId, dependencyId), semaphore);
    }
}

void Renderer3::linkRoot(StageConfig& config) {
    for(size_t i = 0; i < GROUP_SIZE; i++)
        config.waitStages.emplace_back(FRAME_BEGIN_STAGE, &_frameSemaphores[i]);
}


void Renderer3::linkDestination(StageConfig& config) {
    for(size_t i = 0; i < GROUP_SIZE; i++)
        config.signalSemaphores.emplace_back(&_frameSemaphores[i]);
}


void Renderer3::linkFrameBounds(stage_config_map_t& stages, Config::dependencies_t const& dag) {
    auto const roots = dag.roots();
    auto const destinations = dag.destinations();

    for(auto& [id, config] : stages) {
        if(roots.contains(id)) linkRoot(config);
        if(destinations.contains(id)) linkDestination(config);
    }
}
void Renderer3::linkDependencies(stage_config_map_t& stages, Config::dependencies_t const& dag) {
    std::vector semaphores{std::from_range, std::views::transform(_stageSemaphores, [](auto& semaphore) { return &semaphore; })};

    size_t semaphoreCounter = 0;

    for(auto& sourceStage : stages | std::views::values) {
        auto& dependencies = dag.at(sourceStage.uniqueID);
        auto const semaphoreCount = dependencies.size() * GROUP_SIZE;


        auto const stageSemaphores = std::span{&semaphores[semaphoreCounter], semaphoreCount};
        semaphoreCounter += semaphoreCount;

        linkStageDependencies(stages, dag, sourceStage, stageSemaphores);
    }

}
void Renderer3::linkStages(std::span<StageConfig> stages, Config::dependencies_t const& dag) {
    stage_config_map_t stageMap{};
    for(auto& stage : stages) stageMap.emplace(stage.uniqueID, std::move(stage));

    linkFrameBounds(stageMap, dag);
    linkDependencies(stageMap, dag);

}
std::map<uint32_t, size_t> Renderer3::initSharedCmdPools(std::span<StageConfig const> stages) {
    std::map<uint32_t, size_t> familyStages{};

    for(auto const& stage : stages) {
        if(stage.parallel) continue;

        ++familyStages[stage.queue->familyIndex()];
    }

    for(auto& [familyIndex, stageCount] : familyStages) {
        auto const index = _stageCmdPools.size();

        _stageCmdPools.emplace_back(
            *_core,
            CmdPool::Config{
                .maxPrimaryBuffers = stageCount,
                .maxSecondaryBuffers = MAX_SECONDARY_CMD_BUFFERS,
                .queueFamilyIndex = familyIndex,
            }
        );

        stageCount = index;
    }


    return familyStages;
}
size_t Renderer3::initUniqueCmdPool(uint32_t queue_family_index) {
    auto const index = _stageCmdPools.size();

    _stageCmdPools.emplace_back(*_core, CmdPool::Config{
        .maxPrimaryBuffers = 1,
        .maxSecondaryBuffers = MAX_SECONDARY_CMD_BUFFERS,
        .queueFamilyIndex = queue_family_index
    });
    return index;
}

std::vector<size_t> Renderer3::initStagePools(std::span<StageConfig const> stages) {
    std::vector<size_t> poolIndices(stages.size());

    auto familyPoolIndices = initSharedCmdPools(stages);

    for(size_t i = 0; i < stages.size(); i++) {
        auto const& stage = stages[i];
        uint32_t const queueFamilyIndex = stage.queue->familyIndex();

        auto& poolIndex = poolIndices[i];

        if(stage.parallel) poolIndex = initUniqueCmdPool(queueFamilyIndex);
        else poolIndex = familyPoolIndices[queueFamilyIndex];
    }

    return poolIndices;
}
void Renderer3::initRenderStages(std::vector<StageConfig> stage_configs, std::span<size_t const> pool_indices) {
    for(size_t i = 0; i < stage_configs.size(); i++) {
        auto& stageConfig = stage_configs[i];
        auto const poolIndex = pool_indices[i];

        _renderStages.emplace_back(
            *_core,
            RenderStage::Config{
                .queue = stageConfig.queue,
                .cmdPool = &_stageCmdPools[poolIndex],
                .signalSemaphores = std::move(stageConfig.signalSemaphores),
                .waitStages = std::move(stageConfig.waitStages)
            }
        );
    }
}
void Renderer3::createSemaphores() {
    for(auto& semaphore : _frameSemaphores) semaphore.create();

    //BUG check if the semaphores need to be signaled at the beginning

    for(auto& semaphore : _stageSemaphores) semaphore.create();
}
void Renderer3::createCmdPools() {
    for(auto& pools : _stageCmdPools) pools.create();
}
void Renderer3::createStages() {
    for(auto& stage : _renderStages) stage.create();
}

bool Renderer3::created() const { return _frameSemaphores[0].created(); }
}
