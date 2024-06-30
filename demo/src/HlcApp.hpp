#pragma once
#include "render/HlcRenderSystem.hpp"

//TEMP remove this once the camera and input controller are refactored
#include "interface/user/HlcCamera.hpp"
#include "interface/user/HlcInputController.hpp"

#include <cth_engine/cth_engine.hpp>


#include <vector>

#include "vulkan/surface/graphics_core/CthGraphicsCore.hpp"


namespace cth {

class App {
public:
    App();
    ~App() = default;

    void run();



    static constexpr uint32_t WIDTH = 1000;
    static constexpr uint32_t HEIGHT = 1000;

private:
    void initFrame();
    void renderFrame();
    void graphicsPhase() const;


    void initRenderSystem(PrimaryCmdBuffer& cmd_buffer);



    vector<Queue> _queues{Queue{QUEUE_FAMILY_PROPERTY_GRAPHICS | QUEUE_FAMILY_PROPERTY_PRESENT | QUEUE_FAMILY_PROPERTY_TRANSFER}};

    vector<string> _asdf = getRequiredInstanceExtensions();

    std::unique_ptr<Core> _core = make_unique<Core>(BasicCore::Config::Default("demo", "engine", _queues, _asdf));



    std::unique_ptr<DeletionQueue> _deletionQueue = make_unique<DeletionQueue>(_core.get());
    std::unique_ptr<GraphicsSyncConfig> _syncConfig = make_unique<GraphicsSyncConfig>(_core.get(), _deletionQueue.get());

    std::unique_ptr<GraphicsCore> _graphicsCore = make_unique<GraphicsCore>(_core.get(), _deletionQueue.get(), WINDOW_NAME, VkExtent2D{WIDTH, HEIGHT},
        &_queues[0], *_syncConfig.get());

    std::unique_ptr<OSWindow> _window = make_unique<OSWindow>(WINDOW_NAME, WIDTH, HEIGHT, _core->instance());



    std::unique_ptr<Renderer> _renderer = make_unique<Renderer>(_core.get(), _deletionQueue.get(),
        Renderer::Config::Render(_core.get(), _deletionQueue.get(), &_queues[0], _syncConfig.get()));
    InputController _inputController{};
    Camera _camera{};

    std::unique_ptr<RenderSystem> _renderSystem;


    size_t _frameIndex = 0;

    static constexpr std::string_view WINDOW_NAME = "demo";

    [[nodiscard]] static std::vector<std::string> getRequiredInstanceExtensions();

public:
    App(const App& other) = delete;
    App(App&& other) noexcept = delete;
    App& operator=(const App& other) = delete;
    App& operator=(App&& other) noexcept = delete;
};

}
