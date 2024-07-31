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


    void initRenderSystem(vk::PrimaryCmdBuffer& cmd_buffer);



    std::vector<vk::Queue> _queues{
        vk::Queue{vk::QUEUE_FAMILY_PROPERTY_GRAPHICS | vk::QUEUE_FAMILY_PROPERTY_PRESENT | vk::QUEUE_FAMILY_PROPERTY_TRANSFER}
    };

    std::vector<std::string> _glfwExtensions = getRequiredInstanceExtensions();

    std::unique_ptr<vk::Core> _core = std::make_unique<vk::Core>(vk::BasicCore::Config::Default("demo", "engine", _queues, _glfwExtensions));



    std::unique_ptr<vk::DeletionQueue> _deletionQueue = std::make_unique<vk::DeletionQueue>(_core.get());
    std::unique_ptr<vk::GraphicsSyncConfig> _syncConfig = std::make_unique<vk::GraphicsSyncConfig>(_core.get(), _deletionQueue.get());

    std::unique_ptr<vk::GraphicsCore> _graphicsCore = make_unique<vk::GraphicsCore>(_core.get(), _deletionQueue.get(), WINDOW_NAME,
        VkExtent2D{WIDTH, HEIGHT},
        &_queues[0], *_syncConfig);

    std::unique_ptr<vk::OSWindow> _window = make_unique<vk::OSWindow>(WINDOW_NAME, WIDTH, HEIGHT, _core->instance());



    std::unique_ptr<vk::Renderer> _renderer = std::make_unique<vk::Renderer>(_core.get(), _deletionQueue.get(),
        vk::Renderer::Config::Render(&_queues[0], _syncConfig.get()));
    vk::InputController _inputController{};
    vk::Camera _camera{};

    std::unique_ptr<RenderSystem> _renderSystem;


    size_t _frameIndex = 0;

    static constexpr std::string_view WINDOW_NAME = "demo";

    [[nodiscard]] static std::vector<std::string> getRequiredInstanceExtensions();

public:
    App(App const& other) = delete;
    App(App&& other) noexcept = delete;
    App& operator=(App const& other) = delete;
    App& operator=(App&& other) noexcept = delete;
};

}
