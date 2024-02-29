#pragma once
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
    App();
    ~App();

    void run();

    App(const App& other) = delete;
    App(App&& other) = delete;
    App& operator=(const App& other) = delete;
    App& operator=(App&& other) = delete; //copy/move stuff

    inline static constexpr uint32_t WIDTH = 1000;
    inline static constexpr uint32_t HEIGHT = 1000;

    inline static constexpr size_t MAX_STATIC_OBJECTS = 1000;
    inline static constexpr size_t MAX_DYNAMIC_OBJECTS = 1000;
    inline static constexpr size_t MAX_STANDARD_OBJECTS = 2000;

private:
    void reserveObjectMemory();

    void initCamera();
    void initInputController() const;

    void initTrainer();

    void allocateObjectModels();

    static void calculateRenderGroups(array<size_t, 4>& group_sizes, vector<uint32_t>& group_indices,
        const vector<unique_ptr<RenderObject>>& objects);
    void setRenderData();

    void updateFpsDisplay(size_t frame_index, float frame_time) const;

    unique_ptr<Window> hlcWindow = make_unique<Window>(WINDOW_NAME.data(), WIDTH, HEIGHT);
    unique_ptr<Device> hlcDevice = make_unique<Device>(hlcWindow.get());
    InputController inputController{};
    Camera camera{};
    unique_ptr<Renderer> hlcRenderer = make_unique<Renderer>(&camera, hlcWindow.get(), hlcDevice.get());
    MemoryManager memoryManager{hlcDevice};

    RenderSystem renderSystem{hlcDevice, memoryManager, hlcRenderer->swapchainRenderPass(), hlcRenderer->msaaSampleCount()};

    vector<unique_ptr<RenderObject>> inactivePieces{};

    vector<unique_ptr<StandardObject>> standardObjects{};

    vector<unique_ptr<RenderObject>> staticObjects{};
    vector<unique_ptr<RenderObject>> dynamicObjects{};

    RenderSystem::RenderData staticObjectsRenderData{staticObjects};
    RenderSystem::RenderData dynamicObjectsRenderData{dynamicObjects};

    size_t frameIndex = 0;

    inline static constexpr string_view WINDOW_NAME = "tetris_ai";
    inline static constexpr uint32_t POPULATION_SIZE = 100;
};

}
