#pragma once
#include "render/HlcRenderSystem.hpp"

//TEMP remove this once the camera and input controller are refactored
#include "interface/user/HlcCamera.hpp"
#include "interface/user/HlcInputController.hpp"

#include <cth_engine/cth_engine.hpp>


#include <vector>


namespace cth {

class App {
public:
    void run();



    static constexpr uint32_t WIDTH = 1000;
    static constexpr uint32_t HEIGHT = 1000;

private:
    void reserveObjectMemory();

    void initRenderSystem();

    void initCamera();
    void initInputController() const;



    void allocateObjectModels();

    /* static void calculateRenderGroups(array<size_t, 4>& group_sizes, vector<uint32_t>& group_indices,
         const vector<unique_ptr<RenderObject>>& objects);*/
    void setRenderData();

    void updateFpsDisplay(size_t frame_index, float frame_time) const;



    vector<Queue> _queues{Queue{QUEUE_FAMILY_PROPERTY_GRAPHICS | QUEUE_FAMILY_PROPERTY_PRESENT | QUEUE_FAMILY_PROPERTY_TRANSFER}};

    vector<string> _asdf = getRequiredInstanceExtensions();

    std::unique_ptr<Core> _core = make_unique<Core>(BasicCore::Config::Default("demo", "engine", _queues, _asdf));

    std::unique_ptr<OSWindow> _window = make_unique<OSWindow>(WINDOW_NAME, WIDTH, HEIGHT, _core->instance());

    std::unique_ptr<DeletionQueue> _deletionQueue = make_unique<DeletionQueue>(_core.get());


    std::unique_ptr<Renderer> _renderer = make_unique<Renderer>(_core.get(), _deletionQueue.get(), &_camera, _window.get());
    InputController _inputController{};
    Camera _camera{};

    std::unique_ptr<RenderSystem> _renderSystem;


    size_t _frameIndex = 0;

    static constexpr std::string_view WINDOW_NAME = "demo";

    [[nodiscard]] static std::vector<std::string> getRequiredInstanceExtensions();

public:
    App();
    ~App();

    App(const App& other) = delete;
    App(App&& other) = delete;
    App& operator=(const App& other) = delete;
    App& operator=(App&& other) = delete; //copy/move stuff
};

}
