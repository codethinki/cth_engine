#pragma once
#include "vulkan/core/CthInstance.hpp"
#include  "vulkan/core/CthDevice.hpp"
#include "vulkan/core/CthRenderer.hpp"
#include "vulkan/core/CthWindow.hpp"
#include "vulkan/memory/HlcMemoryManager.hpp"
#include "vulkan/user/HlcCamera.hpp"
#include "vulkan/user/HlcInputController.hpp"
#include "vulkan/utils/HlcRenderSystem.hpp"



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


    unique_ptr<Window> hlcWindow = make_unique<Window>(WINDOW_NAME.data(), WIDTH, HEIGHT);
    unique_ptr<Instance> instance = make_unique<Instance>("app", getRequiredInstanceExtensions());
    unique_ptr<Device> hlcDevice = make_unique<Device>(hlcWindow.get(), instance.get());

    InputController inputController{};
    Camera camera{};
    unique_ptr<Renderer> hlcRenderer = make_unique<Renderer>(hlcDevice.get(), &camera, hlcWindow.get());

    RenderSystem renderSystem{hlcDevice.get(), hlcRenderer->swapchainRenderPass(), hlcRenderer->msaaSampleCount()};


    size_t frameIndex = 0;

    inline static bool initialized = false;
    inline static constexpr string_view WINDOW_NAME = "tetris_ai";

    [[nodiscard]] static vector<string> getRequiredInstanceExtensions();

public:
    App();
    ~App();

    static void init();
    static void terminate();

    App(const App& other) = delete;
    App(App&& other) = delete;
    App& operator=(const App& other) = delete;
    App& operator=(App&& other) = delete; //copy/move stuff
};

}
