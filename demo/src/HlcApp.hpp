#pragma once
#include "render/HlcRenderSystem.hpp"

//TEMP remove this once the camera and input controller are refactored
#include "interface/user/HlcCamera.hpp"
#include "interface/user/HlcInputController.hpp"

#include <cth_engine/cth_engine.hpp>


#include <vector>


namespace cth {
using namespace std;

class App {
public:
    void run();



    inline static constexpr uint32_t WIDTH = 1000;
    inline static constexpr uint32_t HEIGHT = 1000;

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


    unique_ptr<Instance> _instance = make_unique<Instance>("app", getRequiredInstanceExtensions());
    unique_ptr<Window> _window = make_unique<Window>(WINDOW_NAME, WIDTH, HEIGHT, _instance.get());
    unique_ptr<PhysicalDevice> _physicalDevice = PhysicalDevice::autoPick(_window->surface(), _instance.get());
    unique_ptr<Device> _device = make_unique<Device>(_physicalDevice.get(), _window->surface(), _instance.get());

    unique_ptr<Renderer> _hlcRenderer = make_unique<Renderer>(_device.get(), &_camera, _window.get());
    InputController _inputController{};
    Camera _camera{};

    unique_ptr<RenderSystem> _renderSystem;


    size_t _frameIndex = 0;

    static constexpr string_view WINDOW_NAME = "demo";

    [[nodiscard]] static vector<string> getRequiredInstanceExtensions();

public:
    App();
    ~App();

    App(const App& other) = delete;
    App(App&& other) = delete;
    App& operator=(const App& other) = delete;
    App& operator=(App&& other) = delete; //copy/move stuff
};

}
