#include "HlcApp.hpp"

#include "render/HlcFrameInfo.hpp"

#include <cth_engine/vulkan/cth_surface.hpp>
#include <cth_engine/vulkan/cth_base.hpp>



namespace cth {

App::App() {
    createRenderer3();
    initFrame();
}

void App::run() {
    cth::log::msg<except::INFO>("starting...");

    while(!_graphicsCore->osWindow()->shouldClose()) {
        glfwPollEvents();

        renderFrame();
    }
    _core->device().waitIdle();

    cth::log::msg<except::INFO>("shutting down...");

    //OldModel::clearModels();
}

void App::createRenderer3() {
    vk::Renderer3::Config config{
        .stages{
            {
                0,
                vk::RenderStageConfig{
                    .queue = &transferQueue()
                }
            },
            {
                1,
                vk::RenderStageConfig{
                    .queue = &renderQueue(),
                    .subStages = 3,
                    .signalSemaphores{std::from_range, _graphicsCore->renderFinishedSemaphores()},
                    .waitStages{std::from_range, _graphicsCore->imageAvailableWaitStages()},
                    .flags = vk::RENDER_STAGE_PARALLEL_FRAMES_IN_FLIGHT_RECORDING | vk::RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING
                }
            }
        },
        .stageDependencies = vk::Renderer3::Config::dependencies_t{
            {{1, 0, VK_PIPELINE_STAGE_TRANSFER_BIT}}
        }

    };


    _renderer3 = std::make_unique<vk::Renderer3>(*_core, _graphicsCore->renderPulse(), config, vk::create);

    _transferStage = &_renderer3->stage(0);
    _graphicsStage = &_renderer3->stage(1);
}
void App::initFrame() {

    _graphicsCore->skipAcquire();

    auto [initCmdBuffer, _] = _transferStage->begin();
    initRenderSystem(*initCmdBuffer);

    _transferStage->submit();
    _graphicsStage->skip();

    _graphicsCore->skipPresent();
}

void App::renderFrame() const {
    _graphicsStage->wait();
    _destructionQueue->next();

    _graphicsCore->acquireFrame();

    _transferStage->skip();

    graphicsPhase();

    _graphicsCore->presentFrame();
}
void App::graphicsPhase() const {
    auto [cmdBuffer, _] = _graphicsStage->begin();

    _graphicsCore->beginWindowPass(cmdBuffer);

    auto const info = FrameInfo{_graphicsCore->pulseVal(), 0.f, cmdBuffer};
    _renderSystem->render(info);

    _graphicsCore->endWindowPass(cmdBuffer);

    _graphicsStage->submit();
}


void App::initRenderSystem(vk::PrimaryCmdBuffer& cmd_buffer) {
    _renderSystem = std::make_unique<RenderSystem>(_core.get(), cmd_buffer, _graphicsCore->swapchainRenderPass(),
        _graphicsCore->msaaSamples());
}



std::vector<std::string> App::getRequiredInstanceExtensions() {
    auto extensions = vk::OSWindow::getGLFWInstanceExtensions();


    return extensions;
}
vk::Queue& App::transferQueue() { return _queues[0]; }
vk::Queue& App::renderQueue() { return _queues[1]; }
vk::Queue& App::presentQueue() { return _queues[2]; }

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
