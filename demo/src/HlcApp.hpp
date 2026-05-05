#pragma once
#include "render/HlcRenderSystem.hpp"

//TEMP remove this once the camera and input controller are refactored
#include "jolly/render/submit/queue.hpp"

#include "render/FrameResources.hpp"

#include "jolly/core/core.hpp"
#include "jolly/render/graph/renderer.hpp"
#include "jolly/user/HlcCamera.hpp"
#include "jolly/user/HlcInputController.hpp"
#include "jolly/utility/GraphicsCore.hpp"



#include <vector>

//TEMP left off here. port GraphicsSyncConfig and Renderer to jly primitives, then see if the project compiles. 
// after that continue implementing the memory management of the model loader with now coroutines

namespace cth {
class FrameResources;
}


namespace cth {

class App {
public:
    App();
    ~App() = default;

    void run() const;



    static constexpr uint32_t WIDTH = 1000;
    static constexpr uint32_t HEIGHT = 1000;
    static constexpr size_t FRAMES_IN_FLIGHT = 2;

private:
    void createRenderer3();

    void initFrame();
    void renderFrame() const;
    void graphicsPhase() const;


    void initRenderSystem(jvk::PrimaryCmdBuffer const& cmd_buffer);

    std::unique_ptr<jly::Core> _core = std::make_unique<jly::Core>(
        jly::CoreConfig{
            "vk_app",
            "engine",
            {
                jly::QueueProperties::TRANSFER | jly::QueueProperties::GRAPHICS,
                jly::QueueProperties::GRAPHICS,
                jly::QueueProperties::PRESENT
            },
        }

    );



    std::vector<std::string> _glfwExtensions = getRequiredInstanceExtensions();


    std::unique_ptr<FrameResources> _resources = std::make_unique<FrameResources>(
        *_core,
        FrameResources::Config{WINDOW_NAME, {WIDTH, HEIGHT}, &_core->queue(2)}
    );

    std::unique_ptr<jly::Renderer> _renderer3;

    jly::RenderStage* _transferStage{};
    jly::RenderStage* _graphicsStage{};


    jly::InputController _inputController{};
    jly::Camera _camera{};

    std::unique_ptr<RenderSystem> _renderSystem;

    size_t _frameCount = 0;

    static constexpr std::string_view WINDOW_NAME = "demo";

    [[nodiscard]] static std::vector<std::string> getRequiredInstanceExtensions();

    [[nodiscard]] jly::Queue const& transferQueue() const;
    [[nodiscard]] jly::Queue const& renderQueue() const;
    [[nodiscard]] jly::Queue const& presentQueue() const;

public
:
    App(App const& other) = delete;
    App(App&& other) noexcept = delete;
    App& operator=(App const& other) = delete;
    App& operator=(App&& other) noexcept = delete;
};

}
