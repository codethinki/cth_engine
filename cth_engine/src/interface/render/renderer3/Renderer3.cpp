#include "Renderer3.hpp"

#include "RenderPulse.hpp"
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
    for(auto const& id : stages | std::views::keys) unusedIds.erase(id);
    for(auto const& id : unusedIds) stageDependencies.erase(id);
}
}

namespace cth::vk {
Renderer3::Renderer3(Core const& core, RenderPulse const& pulse, Config config) : _pulse{&pulse}, _core{&core} {
    config.removeUnusedDependencies();
    Config::debugCheck(config);

    auto& [stages, stageDependencies] = config;
    initFrameSemaphores();
    initDependencySemaphores(stageDependencies.edgeCount());

    linkStages(stages, stageDependencies);

    initRenderStages(stages);
}

Renderer3::Renderer3(Core const& core, RenderPulse const& pulse, Config const& config, create_t) : Renderer3{core, pulse, config} { create(); }

auto Renderer3::create() -> std::map<id_t, RenderStage*> {
    Core::debug_check(*_core);
    optDestroy();

    createSemaphores();
    createStages();

    return stages();
}

void Renderer3::destroy() {
    for(auto& stage : _renderStages | std::views::values) stage.destroy();
    for(auto& semaphore : _stageSemaphores) semaphore.destroy();
    for(auto& semaphore : _frameSemaphores) semaphore.destroy();
}

void Renderer3::initFrameSemaphores() {
    for(size_t i = 0; i < 2 * GROUP_SIZE; i++)
        _frameSemaphores.emplace_back(*_core);
}
void Renderer3::initDependencySemaphores(size_t edges) {
    size_t const semaphores = edges * StageConfig::GROUP_SIZE;
    _stageSemaphores.reserve(semaphores);

    for(size_t i = 0; i < semaphores; ++i) _stageSemaphores.emplace_back(*_core);

}

void Renderer3::linkStageDependencies(
    Config::stage_map_t& stages, Config::dependencies_t const& dag, id_t source_id, std::span<Semaphore*> stage_semaphores) {

    CTH_CRITICAL(!stages.contains(source_id), "stages must contain the source id"){}


    auto const& dependencies = dag.at(source_id);

    for(auto const [dependencyId, dependencySemaphores] : std::views::zip(dependencies, stage_semaphores | std::views::chunk(GROUP_SIZE))) {
        auto& targetStage = stages.at(dependencyId);

        targetStage.signalSemaphores.append_range(dependencySemaphores);
        for(auto semaphore : dependencySemaphores)
            stages.at(source_id).waitStages.emplace_back(dag.annotation(source_id, dependencyId), semaphore);
    }
}

void Renderer3::linkRoot(StageConfig& stage_config) {
    for(size_t i = 0; i < GROUP_SIZE; i++)
        stage_config.waitStages.emplace_back(FRAME_BEGIN_STAGE, &_frameSemaphores[i]);
}


void Renderer3::linkDestination(StageConfig& stage_config) {
    for(size_t i = 0; i < GROUP_SIZE; i++)
        stage_config.signalSemaphores.emplace_back(&_frameSemaphores[i]);
}


void Renderer3::linkFrameBounds(Config::stage_map_t& stages, Config::dependencies_t const& dag) {
    auto const roots = dag.roots();
    auto const destinations = dag.destinations();

    for(auto& [id, config] : stages) {
        if(roots.contains(id)) linkRoot(config);
        if(destinations.contains(id)) linkDestination(config);
    }
}
void Renderer3::linkDependencies(Config::stage_map_t& stages, Config::dependencies_t const& dag) {
    std::vector semaphores{std::from_range, std::views::transform(_stageSemaphores, [](auto& semaphore) { return &semaphore; })};

    size_t semaphoreCounter = 0;

    for(auto const& uniqueID : stages | std::views::keys) {
        auto& dependencies = dag.at(uniqueID);
        auto const semaphoreCount = dependencies.size() * GROUP_SIZE;


        auto const stageSemaphores = std::span{&semaphores[semaphoreCounter], semaphoreCount};
        semaphoreCounter += semaphoreCount;

        linkStageDependencies(stages, dag, uniqueID, stageSemaphores);
    }

}
void Renderer3::linkStages(Config::stage_map_t stages, Config::dependencies_t const& dag) {
    linkFrameBounds(stages, dag);
    linkDependencies(stages, dag);
}

void Renderer3::initRenderStages(Config::stage_map_t const& stage_configs) {
    for(auto& [id, config] : stage_configs)
        _renderStages.emplace(id, RenderStage{*_core, *_pulse, config});
}
void Renderer3::createSemaphores() {
    for(auto& semaphore : _frameSemaphores) semaphore.create();

    //BUG check if the semaphores need to be signaled at the beginning

    for(auto& semaphore : _stageSemaphores) semaphore.create();
}
void Renderer3::createStages() { for(auto& stage : _renderStages | std::views::values) stage.create(); }


bool Renderer3::created() const { return _frameSemaphores[0].created(); }

RenderStage& Renderer3::stage(id_t id) {
    CTH_CRITICAL(!_renderStages.contains(id), "stage id must be present, missing: {}", id) {}
    return _renderStages.at(id);
}

auto Renderer3::stages() -> std::map<id_t, RenderStage*> {
    return std::map{std::from_range, _renderStages | std::views::transform([](auto& pair) { return std::pair{pair.first, &pair.second}; })};
}

}
