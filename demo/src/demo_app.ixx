module;
#include <cstdint>

export module demo.app;

import demo.render.system;
import cth.engine.render;
import cth.vk.submit.queue;
import cth.vk.render.rec.cmd.buffer.primary;
import cth.vk.submit.queue_family;
import cth.engine.render.stage;
import cth.vk.present.core;
import cth.vk.base.core;
import cth.engine.renderer;
import demo.render.system;


import std;

export namespace cth {

class App {
public:
    App();
    ~App() = default;

    void run();



    static constexpr uint32_t WIDTH = 1000;
    static constexpr uint32_t HEIGHT = 1000;

private:
    void createQueues();
    void createPresentCore();
    void createRenderer();


    void initFrame();
    void renderFrame() const;
    void graphicsPhase() const;


    void initRenderSystem(vk::PrimaryCmdBuffer& cmd_buffer);



    std::vector<vk::QueueFamilyProperties> _queueFamilyProperties{
        {vk::QUEUE_FAMILY_PROPERTY_TRANSFER | vk::QUEUE_FAMILY_PROPERTY_GRAPHICS},
        {vk::QUEUE_FAMILY_PROPERTY_GRAPHICS},
        {vk::QUEUE_FAMILY_PROPERTY_PRESENT}
    };
    std::vector<std::string> _glfwExtensions = getRequiredInstanceExtensions();

    std::unique_ptr<vk::Core> _core = std::make_unique<vk::Core>(vk::Core::Config::Default("demo", "engine", _queueFamilyProperties, _glfwExtensions));

    std::vector<vk::Queue> _queues;

    std::unique_ptr<vk::PresentCore> _presentCore;

    std::unique_ptr<vk::Renderer> _renderer3;

    vk::RenderStage* _transferStage{};
    vk::RenderStage* _graphicsStage{};

    /*
    vk::InputController _inputController{};
    vk::Camera _camera{};
    */
    std::unique_ptr<RenderSystem> _renderSystem;

    size_t _frameCount = 0;

    static constexpr std::string_view WINDOW_NAME = "demo";

    [[nodiscard]] static std::vector<std::string> getRequiredInstanceExtensions();

    [[nodiscard]] vk::Queue& transferQueue();
    [[nodiscard]] vk::Queue& renderQueue();
    [[nodiscard]] vk::Queue& presentQueue();

public:
    App(App const& other) = delete;
    App(App&& other) noexcept = delete;
    App& operator=(App const& other) = delete;
    App& operator=(App&& other) noexcept = delete;
};

}
