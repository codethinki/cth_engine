#include "HlcApp.hpp"

#include "vulkan/user/HlcUser.hpp"

#include <chrono>
#include <cstdarg>
#include <iostream>
#include <Windows.h>
#include <glm/glm.hpp>

#define VIEWER standardObjects[0]


namespace cth {
using namespace std;

App::App() {
    reserveObjectMemory();

    initCamera();
    initInputController();

    initTrainer();

    allocateObjectModels();
    setRenderData();
};
App::~App() = default;

void App::run() {
    assert(staticObjects.size() < MAX_STATIC_OBJECTS && dynamicObjects.size() < MAX_DYNAMIC_OBJECTS &&
        standardObjects.size() < MAX_STANDARD_OBJECTS && "element pointers need to stay valid, consider allocating more memory");

    frameIndex = 0;
    auto frameStart = chrono::high_resolution_clock::now();

    while(!hlcWindow.shouldClose()) {
        const auto commandBuffer = hlcRenderer.beginFrame();
        if(commandBuffer == nullptr) continue;

        const auto frameTime = chrono::duration<float, chrono::seconds::period>(chrono::high_resolution_clock::now() - frameStart).count();
        frameStart = chrono::high_resolution_clock::now();

        updateFpsDisplay(frameIndex, frameTime);

        //inputs
        glfwPollEvents();
        if(hlcWindow.focused()) InputController::updateMousePos(hlcWindow.window());
        else InputController::resetMouseDt(hlcWindow.window());


        //camera
        inputController.moveByKeys(frameTime, VIEWER);
        inputController.rotateByMouse(frameTime, VIEWER);

        camera.setViewYXZ(VIEWER->transform.translation, VIEWER->transform.rotation);

        renderSystem.updateDynamicChunks();

        FrameInfo info = {hlcRenderer.getFrameIndex(), 0.f, commandBuffer, camera};

        hlcRenderer.beginSwapchainRenderPass(info.commandBuffer);
        renderSystem.render(staticObjectsRenderData, info);
        hlcRenderer.endSwapchainRenderPass(info.commandBuffer);
        hlcRenderer.endFrame();

        frameIndex++;
    }
    vkDeviceWaitIdle(hlcDevice.device());

    OldModel::clearModels();
}

void App::reserveObjectMemory() {
    standardObjects.reserve(MAX_STANDARD_OBJECTS);
    dynamicObjects.reserve(MAX_DYNAMIC_OBJECTS);
    staticObjects.reserve(MAX_STATIC_OBJECTS);
}

void App::initCamera() {
    standardObjects.clear();
    standardObjects.emplace_back(new User{Transform{{0, 0, -2.5f}}});

    camera.setViewYXZ(VIEWER->transform.translation, VIEWER->transform.rotation);
    camera.setPerspectiveProjection(glm::radians(80.f), hlcRenderer.screenRatio(), 0.01f, 10.f);
}
void App::initInputController() const {
    glfwSetInputMode(hlcWindow.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    InputController::resetMouseDt(hlcWindow.window());
}

void App::allocateObjectModels() {
    //TEMP update this

    //memoryManager.allocate(staticObjects);
    memoryManager.allocateBuffers();

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
    calculateRenderGroups(staticObjectsRenderData.groupSizes, staticObjectsRenderData.groupIndices, staticObjects);

    //dynamic objects render data
    calculateRenderGroups(dynamicObjectsRenderData.groupSizes, dynamicObjectsRenderData.groupIndices, dynamicObjects);
}

void App::updateFpsDisplay(const size_t frame_index, const float frame_time) const {
    static float frameTimeSum = 0;
    static size_t oldFrameIndex = frame_index;

    frameTimeSum += frame_time;
    if(frameTimeSum < 1) return;

    const string x = "fps: " + to_string(static_cast<float>(frame_index - oldFrameIndex) / frameTimeSum);
    frameTimeSum = 0;
    oldFrameIndex = frame_index;

    glfwSetWindowTitle(hlcWindow.window(), x.c_str());
}

}
