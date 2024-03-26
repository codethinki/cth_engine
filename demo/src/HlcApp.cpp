#include "HlcApp.hpp"

#include "render/HlcFrameInfo.hpp"

#include <chrono>
#include <Windows.h>


namespace cth {
using namespace std;


void App::run() {

    frameIndex = 0;
    auto frameStart = chrono::high_resolution_clock::now();

    while(!window->shouldClose()) {
        const auto commandBuffer = hlcRenderer->beginFrame();
        if(commandBuffer == nullptr) continue;

        const auto frameTime = chrono::duration<float, chrono::seconds::period>(chrono::high_resolution_clock::now() - frameStart).count();
        frameStart = chrono::high_resolution_clock::now();

        updateFpsDisplay(frameIndex, frameTime);

        //inputs
        glfwPollEvents();
        /* if(hlcWindow->focused()) InputController::updateMousePos(hlcWindow->window());
         else InputController::resetMouseDt(hlcWindow->window());*/


        //camera
        /* inputController.moveByKeys(frameTime, VIEWER);
         inputController.rotateByMouse(frameTime, VIEWER);*/

        /*  camera.setViewYXZ(VIEWER->transform.translation, VIEWER->transform.rotation);*/

        /* renderSystem.updateDynamicChunks();*/

        FrameInfo info = {hlcRenderer->frameIndex(), 0.f, commandBuffer};

        hlcRenderer->beginSwapchainRenderPass(info.commandBuffer);
        renderSystem.render(info);
        hlcRenderer->endSwapchainRenderPass(info.commandBuffer);
        hlcRenderer->endFrame();

        frameIndex++;
    }
    vkDeviceWaitIdle(device->device());

    //OldModel::clearModels();
}

void App::reserveObjectMemory() {
    /*standardObjects.reserve(MAX_STANDARD_OBJECTS);
    dynamicObjects.reserve(MAX_DYNAMIC_OBJECTS);
    staticObjects.reserve(MAX_STATIC_OBJECTS);*/
}

void App::initCamera() {
    /* standardObjects.clear();
     standardObjects.emplace_back(new User{Transform{{0, 0, -2.5f}}});
 
     camera.setViewYXZ(VIEWER->transform.translation, VIEWER->transform.rotation);
     camera.setPerspectiveProjection(glm::radians(80.f), hlcRenderer.screenRatio(), 0.01f, 10.f);*/
}
void App::initInputController() const {
    //glfwSetInputMode(hlcWindow.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    //InputController::resetMouseDt(hlcWindow.window());
}

void App::allocateObjectModels() {
    //TEMP update this

    //memoryManager.allocate(staticObjects);
    //memoryManager.allocateBuffers();

}

void App::calculateRenderGroups(array<size_t, 4>& group_sizes, vector<uint32_t>& group_indices,
    const vector<unique_ptr<RenderObject>>& objects) {
    vector<uint32_t> staticGroup{};
    vector<uint32_t> staticVerticesGroup{};
    vector<uint32_t> staticIndicesGroup{};
    vector<uint32_t> dynamicGroup{};

    for(int i = 0; i < objects.size(); i++) {
        const int renderGroup = objects[i]->renderGroupFlags;

        assert(renderGroup != RenderObject::RENDER_GROUP_INVALID && "render Group must be defined");

        if(renderGroup & RenderObject::RENDER_GROUP_STATIC) staticGroup.push_back(i);
        if(renderGroup & RenderObject::RENDER_GROUP_STATIC_VERTICES) staticVerticesGroup.push_back(i);
        if(renderGroup & RenderObject::RENDER_GROUP_STATIC_INDICES) staticIndicesGroup.push_back(i);
        if(renderGroup & RenderObject::RENDER_GROUP_DYNAMIC) dynamicGroup.push_back(i);
    }
    group_sizes = {staticGroup.size(), staticVerticesGroup.size(), staticIndicesGroup.size(), dynamicGroup.size()};

    group_indices.insert(group_indices.end(), staticGroup.begin(), staticGroup.end());
    group_indices.insert(group_indices.end(), staticVerticesGroup.begin(), staticVerticesGroup.end());
    group_indices.insert(group_indices.end(), staticIndicesGroup.begin(), staticIndicesGroup.end());
    group_indices.insert(group_indices.end(), dynamicGroup.begin(), dynamicGroup.end());
}

void App::setRenderData() {
    //static objects render data
    //calculateRenderGroups(staticObjectsRenderData.groupSizes, staticObjectsRenderData.groupIndices, staticObjects);

    //dynamic objects render data
    //calculateRenderGroups(dynamicObjectsRenderData.groupSizes, dynamicObjectsRenderData.groupIndices, dynamicObjects);
}

void App::updateFpsDisplay(const size_t frame_index, const float frame_time) const {
    static float frameTimeSum = 0;
    static size_t oldFrameIndex = frame_index;

    frameTimeSum += frame_time;
    if(frameTimeSum < 1) return;

    const string x = "fps: " + std::to_string(static_cast<float>(frame_index - oldFrameIndex) / frameTimeSum);
    frameTimeSum = 0;
    oldFrameIndex = frame_index;

    glfwSetWindowTitle(window->window(), x.c_str());
}
vector<string> App::getRequiredInstanceExtensions() {
    auto extensions = Window::getGLFWInstanceExtensions();
    return extensions;
}

App::App() {
    reserveObjectMemory();

    initCamera();
    initInputController();

    allocateObjectModels();
    setRenderData();
};
App::~App() {
    log::msg<except::INFO>("destroying app");
}

void App::init() {
    cth::log::msg<except::INFO>("initializing...");

    Window::init();

    log::msg<except::INFO>("initialized app");
    initialized = true;
}
void App::terminate() {
    cth::log::msg<except::INFO>("terminating...");

    Window::terminate();

    log::msg<except::INFO>("terminated app");
    initialized = false;
}


}
