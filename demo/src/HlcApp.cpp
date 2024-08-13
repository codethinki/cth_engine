#include "HlcApp.hpp"

#include "render/HlcFrameInfo.hpp"


namespace cth {

App::App() { initFrame(); }

void App::run() {
    cth::log::msg<except::INFO>("starting...");

    while(!_graphicsCore->osWindow()->shouldClose()) {
        glfwPollEvents();

        renderFrame();
    }
    _core->device()->waitIdle();

    cth::log::msg<except::INFO>("shutting down...");

    //OldModel::clearModels();
}

void App::initFrame() {
    _graphicsCore->acquireFrame(_destructionQueue.get());

    auto* initCmdBuffer = _renderer->begin<vk::Renderer::PHASE_TRANSFER>();
    initRenderSystem(*initCmdBuffer);
    _renderer->end<vk::Renderer::PHASE_TRANSFER>();

    _renderer->skip<vk::Renderer::PHASE_GRAPHICS>();

    _graphicsCore->presentFrame();

    _frameCount++;
}

void App::renderFrame() {
    _renderer->wait();

    _graphicsCore->acquireFrame(_destructionQueue.get());

    _renderer->skip<vk::Renderer::PHASE_TRANSFER>();

    graphicsPhase();

    _graphicsCore->presentFrame();

    _frameCount++;
}
void App::graphicsPhase() const {
    auto* cmdBuffer = _renderer->begin<vk::Renderer::PHASE_GRAPHICS>();


    FrameInfo info = {_renderer->frameIndex(), 0.f, cmdBuffer};

    _graphicsCore->beginWindowPass(cmdBuffer);

    _renderSystem->render(info);

    _graphicsCore->endWindowPass(cmdBuffer);


    _renderer->end<vk::Renderer::PHASE_GRAPHICS>();
}


void App::initRenderSystem(vk::PrimaryCmdBuffer& cmd_buffer) {
    _renderSystem = std::make_unique<RenderSystem>(_core.get(), _destructionQueue.get(), cmd_buffer, _graphicsCore->swapchainRenderPass(),
        _graphicsCore->msaaSamples());
}



std::vector<std::string> App::getRequiredInstanceExtensions() {
    auto extensions = vk::OSWindow::getGLFWInstanceExtensions();
    return extensions;
}

}

//TEMP old code
//inputs
/* if(hlcWindow->focused()) InputController::updateMousePos(hlcWindow->window());
 else InputController::resetMouseDt(hlcWindow->window());*/


//camera
/* inputController.moveByKeys(frameTime, VIEWER);
 inputController.rotateByMouse(frameTime, VIEWER);*/

/*  camera.setViewYXZ(VIEWER->transform.translation, VIEWER->transform.rotation);*/

/* renderSystem.updateDynamicChunks();*/

//void App::setRenderData() {
//    //static objects render data
//    //calculateRenderGroups(staticObjectsRenderData.groupSizes, staticObjectsRenderData.groupIndices, staticObjects);
//
//    //dynamic objects render data
//    //calculateRenderGroups(dynamicObjectsRenderData.groupSizes, dynamicObjectsRenderData.groupIndices, dynamicObjects);
//}

//void App::calculateRenderGroups(array<size_t, 4>& group_sizes, vector<uint32_t>& group_indices,
//    const vector<unique_ptr<RenderObject>>& objects) {
//    vector<uint32_t> staticGroup{};
//    vector<uint32_t> staticVerticesGroup{};
//    vector<uint32_t> staticIndicesGroup{};
//    vector<uint32_t> dynamicGroup{};
//
//    for(int i = 0; i < objects.size(); i++) {
//        const int renderGroup = objects[i]->renderGroupFlags;
//
//        assert(renderGroup != RenderObject::RENDER_GROUP_INVALID && "render Group must be defined");
//
//        if(renderGroup & RenderObject::RENDER_GROUP_STATIC) staticGroup.push_back(i);
//        if(renderGroup & RenderObject::RENDER_GROUP_STATIC_VERTICES) staticVerticesGroup.push_back(i);
//        if(renderGroup & RenderObject::RENDER_GROUP_STATIC_INDICES) staticIndicesGroup.push_back(i);
//        if(renderGroup & RenderObject::RENDER_GROUP_DYNAMIC) dynamicGroup.push_back(i);
//    }
//    group_sizes = {staticGroup.size(), staticVerticesGroup.size(), staticIndicesGroup.size(), dynamicGroup.size()};
//
//    group_indices.insert(group_indices.end(), staticGroup.begin(), staticGroup.end());
//    group_indices.insert(group_indices.end(), staticVerticesGroup.begin(), staticVerticesGroup.end());
//    group_indices.insert(group_indices.end(), staticIndicesGroup.begin(), staticIndicesGroup.end());
//    group_indices.insert(group_indices.end(), dynamicGroup.begin(), dynamicGroup.end());
//}

//
