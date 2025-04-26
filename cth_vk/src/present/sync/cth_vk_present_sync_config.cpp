module cth.vk.present.sync.config;


namespace cth::vk {

PresentSyncConfig::PresentSyncConfig(Core const& core) : _core{&core} {}
PresentSyncConfig::PresentSyncConfig(Core const& core, State state) : PresentSyncConfig{core} { wrap(std::move(state)); }
PresentSyncConfig::PresentSyncConfig(Core const& core, create_t) : PresentSyncConfig{core} { create(); }
PresentSyncConfig::~PresentSyncConfig() { optDestroy(); }
void PresentSyncConfig::wrap(State state) {
    State::debug_check(state);
    optDestroy();
    _pulse = std::move(state.pulse);
    _renderFinishedSemaphores = std::move(state.renderFinishedSemaphores);
    _imageAvailableSemaphores = std::move(state.imageAvailableSemaphores);
}
void PresentSyncConfig::create() {
    optDestroy();

    for(auto& uniquePtr : _renderFinishedSemaphores) //TODO refactor this to init and make create only create
        uniquePtr = std::make_unique<Semaphore>(*_core, vk::create);
    for(auto& uniquePtr : _imageAvailableSemaphores)
        uniquePtr = std::make_unique<Semaphore>(*_core, vk::create);
}
void PresentSyncConfig::destroy() {
    debug_check(*this);

    for(auto& semaphore : _renderFinishedSemaphores) semaphore = nullptr; //TODO same as create()
    for(auto& semaphore : _imageAvailableSemaphores) semaphore = nullptr;


    _pulse.reset();
}
PresentSyncConfig::State PresentSyncConfig::release() {
    debug_check(*this);

    return State{
        .imageAvailableSemaphores = std::move(_imageAvailableSemaphores),
        .renderFinishedSemaphores = std::move(_renderFinishedSemaphores)
    };
}



auto PresentSyncConfig::renderFinishedSemaphores() const -> std::array<Semaphore*, SET_SIZE> {
    debug_check(*this);


    std::array<Semaphore*, SET_SIZE> semaphores{};
    for(auto [src, dst] : std::views::zip(_renderFinishedSemaphores, semaphores)) dst = src.get();
    return semaphores;
}
auto PresentSyncConfig::imageAvailableSemaphores() const -> std::array<Semaphore*, SET_SIZE> {
    debug_check(*this);

    std::array<Semaphore*, SET_SIZE> semaphores{};
    for(auto [src, dst] : std::views::zip(_imageAvailableSemaphores, semaphores)) dst = src.get();
    return semaphores;
}
auto PresentSyncConfig::imageAvailableWaitStages() const -> std::vector<PipelineWaitStage> {
    auto const semaphores = imageAvailableSemaphores();
    return {
        std::from_range,
        semaphores | std::views::transform([](Semaphore const* ptr) { return PipelineWaitStage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ptr}; })
    };


}
Semaphore* PresentSyncConfig::renderFinishedSemaphore(size_t index) const { return _renderFinishedSemaphores[index].get(); }
Semaphore* PresentSyncConfig::imageAvailableSemaphore(size_t index) const { return _imageAvailableSemaphores[index].get(); }

}


//State

namespace cth::vk {
void PresentSyncConfig::State::debug_check(State const& state) {
    for(auto& semaphore : state.imageAvailableSemaphores)
        Semaphore::debug_check(*semaphore);
    for(auto& semaphore : state.renderFinishedSemaphores)
        Semaphore::debug_check(*semaphore);
}

}
