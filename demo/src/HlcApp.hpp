#pragma once
#include "render/HlcRenderSystem.hpp"

//TEMP remove this once the camera and input controller are refactored
#include "render/FrameResources.hpp"

#include "jolly/render/renderer.hpp"
#include "jolly/user/HlcCamera.hpp"
#include "jolly/user/HlcInputController.hpp"
#include "jolly/utility/GraphicsCore.hpp"



#include <vector>



namespace cth {
class FrameResources;
}


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


    void initRenderSystem(jvk::PrimaryCmdBuffer const& cmd_buffer);



    std::vector<jvk::Queue> _queues{
        jvk::Queue{jvk::QUEUE_FAMILY_PROPERTY_TRANSFER | jvk::QUEUE_FAMILY_PROPERTY_GRAPHICS},
        jvk::Queue{jvk::QUEUE_FAMILY_PROPERTY_GRAPHICS},
        jvk::Queue{jvk::QUEUE_FAMILY_PROPERTY_PRESENT}
    };
    std::array<jvk::Core::queue_set_t, 2> _queueSets{
        {
            {{0}, {1}, {2}},
            {{0, 1, 2}}
        }
    };



    std::vector<std::string> _glfwExtensions = getRequiredInstanceExtensions();

    std::unique_ptr<jvk::Core> _core = std::make_unique<jvk::Core>(
        jvk::Core::Config{
            "demo",
            "engine",
            _queues,
            _glfwExtensions,
            _queueSets,
        }
    );

    cth::move_ptr<jvk::DestructionQueue> _destructionQueue = _core->destructionQueue();


    std::unique_ptr<FrameResources> _resources = std::make_unique<FrameResources>(
        *_core,
        FrameResources::Config{WINDOW_NAME, {WIDTH, HEIGHT}, _queues[2]}
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

    [[nodiscard]] jvk::Queue& transferQueue();
    [[nodiscard]] jvk::Queue& renderQueue();
    [[nodiscard]] jvk::Queue& presentQueue();

public
:
    App(App const& other) = delete;
    App(App&& other) noexcept = delete;
    App& operator=(App const& other) = delete;
    App& operator=(App&& other) noexcept = delete;
};

}
