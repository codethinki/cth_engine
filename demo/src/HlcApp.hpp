#pragma once
#include "render/HlcRenderSystem.hpp"

//TEMP remove this once the camera and input controller are refactored
#include "src/interface/render/renderer3/Renderer3.hpp"
#include "src/interface/user/HlcCamera.hpp"
#include "src/interface/user/HlcInputController.hpp"

#include <cth_engine/interface/render.hpp>


#include <vector>

#include "src/vulkan/surface/graphics_core/CthGraphicsCore.hpp"


namespace cth {

class App {
public:
    App();
    ~App() = default;

    void run();



    static constexpr uint32_t WIDTH = 1000;
    static constexpr uint32_t HEIGHT = 1000;

private:
    void createRenderer3();


    void initFrame();
    void renderFrame() const;
    void graphicsPhase() const;


    void initRenderSystem(vk::PrimaryCmdBuffer& cmd_buffer);



    std::vector<vk::Queue> _queues{
        vk::Queue{vk::QUEUE_FAMILY_PROPERTY_TRANSFER | vk::QUEUE_FAMILY_PROPERTY_GRAPHICS},
        vk::Queue{vk::QUEUE_FAMILY_PROPERTY_GRAPHICS},
        vk::Queue{vk::QUEUE_FAMILY_PROPERTY_PRESENT}
    };

    std::vector<std::string> _glfwExtensions = getRequiredInstanceExtensions();

    std::unique_ptr<vk::Core> _core = std::make_unique<vk::Core>(vk::Core::Config::Default("demo", "engine", _queues, _glfwExtensions));

    cth::move_ptr<vk::DestructionQueue> _destructionQueue = _core->destructionQueue();

    std::unique_ptr<vk::GraphicsCore> _graphicsCore = make_unique<vk::GraphicsCore>(*_core, WINDOW_NAME, VkExtent2D{WIDTH, HEIGHT}, _queues[2]);

    std::unique_ptr<vk::Renderer3> _renderer3;

    vk::RenderStage* _transferStage{};
    vk::RenderStage* _graphicsStage{};


    vk::InputController _inputController{};
    vk::Camera _camera{};

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
