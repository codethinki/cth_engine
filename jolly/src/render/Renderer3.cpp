#include "jolly/render/Renderer3.hpp"

#include "jolly/render/RenderPulse.hpp"
#include "jolly/render/RenderStage.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue/submit_info.hpp"
#include "jvk/render/cmd/cmd_pool.hpp"
#include "jvk/render/ctrl/semaphore.hpp"
#include "jvk/utility/constants.hpp"

#include <cth/algorithm/views.hpp>

namespace jvk {
void Renderer3Config::removeUnusedDependencies() {
    auto unusedIds = stageDependencies.nodes();
    for(auto const& id : stages | std::views::keys) unusedIds.erase(id);
    for(auto const& id : unusedIds) stageDependencies.erase(id);
}
}

namespace jvk {
Renderer3::Renderer3(Core const& core, RenderPulse const& pulse, Config config) : _core{&core},
    _pulse{&pulse} {
    config.removeUnusedDependencies();
    Config::debugCheck(config);

    auto& [stages, stageDependencies] = config;
    initDependencySemaphores(stageDependencies.edgeCount());

    linkDependencies(stages, stageDependencies);

    initRenderStages(stages);
}

Renderer3::Renderer3(Core const& core, RenderPulse const& pulse, Config const& config, create_t) : Renderer3{
    core, pulse, config} { create(); }

Renderer3::~Renderer3() { optDestroy(); }

auto Renderer3::create() -> std::map<id_t, RenderStage*> {
    Core::debug_check(*_core);
    optDestroy();

    createSemaphores();
    createStages();

    _created = true;

    return stages();
}

void Renderer3::destroy() {
    for(auto& stage : _renderStages | std::views::values) stage.destroy();
    for(auto& semaphore : _stageSemaphores) semaphore.destroy();

    _created = false;
}

void Renderer3::initDependencySemaphores(size_t edges) {
    size_t const semaphores = edges * StageConfig::GROUP_SIZE;
    _stageSemaphores.reserve(semaphores);

    for(size_t i = 0; i < semaphores; ++i) _stageSemaphores.emplace_back(*_core);

}

void Renderer3::linkStageDependencies(Config::stage_map_t& stages, Config::dependencies_t const& dag,
    id_t source_id,
    std::span<Semaphore*> stage_semaphores) {

    CTH_CRITICAL(!stages.contains(source_id), "stages must contain the source id") {}


    auto const& dependencies = dag.at(source_id);

    for(auto const [dependencyId, dependencySemaphores] : std::views::zip(dependencies,
            stage_semaphores | std::views::chunk(GROUP_SIZE))) {
        auto& targetStage = stages.at(dependencyId);

        targetStage.signalSemaphores.append_range(dependencySemaphores);
        for(auto& semaphore : dependencySemaphores)
            stages.at(source_id).waitStages.emplace_back(dag.annotation(source_id, dependencyId), semaphore);
    }
}


void Renderer3::linkDependencies(Config::stage_map_t& stages, Config::dependencies_t const& dag) {
    std::vector semaphores{std::from_range, _stageSemaphores | cth::views::to_ptr_range};

    size_t semaphoreCounter = 0;

    for(auto const& uniqueID : stages | std::views::keys) {
        auto& dependencies = dag.at(uniqueID);
        auto const semaphoreCount = dependencies.size() * GROUP_SIZE;


        auto const stageSemaphores = std::span{&semaphores[semaphoreCounter], semaphoreCount};
        semaphoreCounter += semaphoreCount;

        linkStageDependencies(stages, dag, uniqueID, stageSemaphores);
    }

}

void Renderer3::initRenderStages(Config::stage_map_t const& stage_configs) {
    for(auto& [id, config] : stage_configs)
        _renderStages.emplace(id, RenderStage{*_core, *_pulse, config});
}

void Renderer3::createSemaphores() {
    for(auto& semaphore : _stageSemaphores) semaphore.create();
}

void Renderer3::createStages() { for(auto& stage : _renderStages | std::views::values) stage.create(); }


RenderStage& Renderer3::stage(id_t id) {
    CTH_CRITICAL(!_renderStages.contains(id), "stage id must be present, missing: {}", id) {}
    return _renderStages.at(id);
}

auto Renderer3::stages() -> std::map<id_t, RenderStage*> {
    return std::map{std::from_range,
        _renderStages | std::views::transform([](auto& pair) {
            return std::pair{pair.first, &pair.second};
        })};
}

}
