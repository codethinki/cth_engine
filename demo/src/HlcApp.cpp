#include "HlcApp.hpp"

#include "jolly/render/graph/RenderStage.hpp"
#include "jolly/render/submit/cmd/primary_cmd_buffer.hpp"
#include "jolly/utility/CthOSWindow.hpp"

#include "jolly/utility/types.hpp"
#include "render/HlcFrameInfo.hpp"

#include "jvk/base/device.hpp"


namespace cth {

App::App() {
    createRenderer3();
    initFrame();
}

void App::run() const {
    cth::log::msg<except::INFO>("starting...");

    while(!_resources->shouldClose()) {
        glfwPollEvents();

        renderFrame();
    }
    _core->wait();

    cth::log::msg<except::INFO>("shutting down...");

    //OldModel::clearModels();
}


void App::createRenderer3() {
    jly::Renderer::Config const config{
        .stages{
            {
                0,
                jly::RenderStageConfig{
                    .queue = &transferQueue()
                }
            },
            {
                1,
                jly::RenderStageConfig{
                    .queue = &renderQueue(),
                    .subStages = 3,
                    .signalSemaphores{std::from_range, _resources->renderFinishedSemaphores()},
                    .waitStages{_resources->syncConfig().imageAvailableWaitStages()},
                    .flags = jly::RenderStageFlags::PARALLEL_FRAMES_IN_FLIGHT_RECORDING
                    | jly::RenderStageFlags::PARALLEL_SUB_STAGE_RECORDING
                }
            }
        },
        .stageDependencies = jly::Renderer::Config::dependencies_t{
            {{1, 0, jly::PipelineStageFlags::TRANSFER_BIT}}
        }

    };


    _renderer3 = std::make_unique<jly::Renderer>(*_core, _resources->renderPulse(), config, jly::create);

    _transferStage = &_renderer3->stage(0);
    _graphicsStage = &_renderer3->stage(1);
}

void App::initFrame() {
    _resources->skipAcquire();

    auto [initCmdBuffer, _] = _transferStage->begin();
    initRenderSystem(initCmdBuffer->raw());

    _transferStage->submit();
    _graphicsStage->skip();

    _resources->skipPresent();
}

void App::renderFrame() const {
    _graphicsStage->wait();
    _core->tickFrame();

    _resources->acquireFrame();

    _transferStage->skip();

    graphicsPhase();

    _resources->presentFrame();
}

void App::graphicsPhase() const {
    auto [cmdBuffer, _] = _graphicsStage->begin();

    _resources->beginRenderPass(cmdBuffer->raw());

    auto const info = FrameInfo{_resources->renderPulse().get(), 0.f, &cmdBuffer->raw()};
    _renderSystem->render(info);

    _resources->endRenderPass(cmdBuffer->raw());

    _graphicsStage->submit();
}


void App::initRenderSystem(jvk::PrimaryCmdBuffer const& cmd_buffer) {
    _renderSystem = std::make_unique<RenderSystem>(
        &_core->raw(),
        cmd_buffer,
        _resources->renderPass(),
        _resources->msaaSampleCount()
    );
}



std::vector<std::string> App::getRequiredInstanceExtensions() {
    auto extensions = jly::OSWindow::getGLFWInstanceExtensions();


    return extensions;
}

jly::Queue const& App::transferQueue() const { return _core->queue(0); }
jly::Queue const& App::renderQueue() const { return _core->queue(1); }
jly::Queue const& App::presentQueue() const { return _core->queue(2); }

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
