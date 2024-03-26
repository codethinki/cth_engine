#pragma once
#include "render/HlcRenderSystem.hpp"

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

    void initCamera();
    void initInputController() const;


    void allocateObjectModels();

    static void calculateRenderGroups(array<size_t, 4>& group_sizes, vector<uint32_t>& group_indices,
        const vector<unique_ptr<RenderObject>>& objects);
    void setRenderData();

    void updateFpsDisplay(size_t frame_index, float frame_time) const;


    unique_ptr<Instance> instance = make_unique<Instance>("app", getRequiredInstanceExtensions());
    unique_ptr<Window> window = make_unique<Window>(instance.get(), WINDOW_NAME.data(), WIDTH, HEIGHT);
    unique_ptr<Device> device = make_unique<Device>(window.get(), instance.get());

    InputController inputController{};
    Camera camera{};
    unique_ptr<Renderer> hlcRenderer = make_unique<Renderer>(device.get(), &camera, window.get());

    RenderSystem renderSystem{device.get(), hlcRenderer->swapchainRenderPass(), hlcRenderer->msaaSampleCount()};


    size_t frameIndex = 0;

    inline static constexpr string_view WINDOW_NAME = "tetris_ai";

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
