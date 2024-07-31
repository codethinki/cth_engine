#include "CthRenderer.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/cmd/CthCmdPool.hpp"
#include "vulkan/surface/CthOSWindow.hpp"
#include "vulkan/surface/graphics_core/CthGraphicsSyncConfig.hpp"


namespace cth::vk {
using std::vector;

Renderer::Renderer(BasicCore const* core, DeletionQueue* deletion_queue, Config const& config) : _core(core), _deletionQueue(deletion_queue),
    _queues(config.queues()) { init(config); }
Renderer::~Renderer() {

    std::ranges::fill(_cmdBuffers, nullptr);
    std::ranges::fill(_cmdPools, nullptr);
}


void Renderer::wait() const {
    auto const result = semaphore()->wait();
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to wait for current phase")
        throw except::vk_result_exception{result, details->exception()};
}

void Renderer::init(Config const& config) {
    createCmdPools();
    createPrimaryCmdBuffers();
    createSyncObjects();

    createSubmitInfos(config);
}

void Renderer::createCmdPools() {
    for(size_t i = PHASES_FIRST; i < PHASES_SIZE; ++i)
        _cmdPools[i] = std::make_unique<CmdPool>(_core, CmdPool::Config::Default(_queues[i]->familyIndex(), constants::FRAMES_IN_FLIGHT + 1, 0));
}
void Renderer::createPrimaryCmdBuffers() {
    for(size_t i = PHASES_FIRST; i < PHASES_SIZE; ++i)
        for(size_t j = 0; j < constants::FRAMES_IN_FLIGHT; ++j)
            _cmdBuffers[i * PHASES_SIZE + j] = std::make_unique<PrimaryCmdBuffer>(_cmdPools[i].get());
}
void Renderer::createSyncObjects() {
    std::ranges::transform(_semaphores, _semaphores.begin(),
        [this](auto&) { return std::make_unique<TimelineSemaphore>(_core, _deletionQueue); }
        );
}


std::array<PipelineWaitStage, constants::FRAMES_IN_FLIGHT> Renderer::createWaitSet() const {
    std::array<PipelineWaitStage, constants::FRAMES_IN_FLIGHT> stages{};
    for(auto [stage, semaphore] : std::views::zip(stages, _semaphores)) {
        stage.stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        stage.semaphore = semaphore.get();
    }
    return stages;
}
std::array<BasicSemaphore*, constants::FRAMES_IN_FLIGHT> Renderer::createSignalSet() const {
    std::array<BasicSemaphore*, constants::FRAMES_IN_FLIGHT> semaphores{};
    for(auto [dst, src] : std::views::zip(semaphores, _semaphores))
        dst = src.get();

    return semaphores;
}


void Renderer::createSubmitInfos(Config config) {
    auto const& signalSet = createSignalSet();
    auto const& waitSet = createWaitSet();

    config.addWaitSets<PHASE_TRANSFER>(waitSet);
    config.addSignalSets<PHASE_TRANSFER>(signalSet);

    config.addWaitSets<PHASE_GRAPHICS>(waitSet);
    config.addSignalSets<PHASE_GRAPHICS>(signalSet);

    static_assert(PHASES_SIZE == 2, "missing phases to inject into config");

    vector<PrimaryCmdBuffer const*> cmdBuffers(_cmdBuffers.size());
    std::ranges::transform(_cmdBuffers, cmdBuffers.begin(), [](auto const& buffer) { return buffer.get(); });

    _submitInfos = config.createSubmitInfos(cmdBuffers);
}



DeletionQueue* Renderer::deletionQueue() const { return _deletionQueue; }

} // namespace cth

//Config

namespace cth::vk {
Renderer::Config Renderer::Config::Render(Queue const* graphics_queue,
    BasicGraphicsSyncConfig* sync_config) {
    Config config{};
    config.addQueue<PHASE_TRANSFER>(graphics_queue)
          .addQueue<PHASE_GRAPHICS>(graphics_queue)
          .addWaitSets<PHASE_GRAPHICS>(sync_config->imageAvailableSemaphores, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
          .addSignalSets<PHASES_LAST>(sync_config->renderFinishedSemaphores);
    return config;
}
//Renderer::Config::Config(const BasicCore* core, DeletionQueue* deletion_queue) : _core{core}, _deletionQueue{deletion_queue} {
//    DEBUG_CHECK_CORE(core);
//    DEBUG_CHECK_DELETION_QUEUE(deletion_queue);
//}



auto Renderer::Config::createSubmitInfos(std::span<PrimaryCmdBuffer const* const> const cmd_buffers) const
    -> std::vector<Queue::SubmitInfo> {
    DEBUG_CHECK_RENDERER_CONFIG_SET_SIZE(cmd_buffers);

    auto phaseBuffers = cmd_buffers | std::views::chunk(SET_SIZE);


    std::array phaseSubmitInfos = {
        createPhaseSubmitInfos<PHASE_TRANSFER>(phaseBuffers[PHASE_TRANSFER]),
        createPhaseSubmitInfos<PHASE_GRAPHICS>(phaseBuffers[PHASE_GRAPHICS]),
    };
    static_assert(phaseSubmitInfos.size() == PHASES_SIZE, "missing phases");
    static_assert(PHASES_SIZE == 2, "missing phases to add");

    auto view = phaseSubmitInfos | std::views::join;

    std::vector<Queue::SubmitInfo> result{};
    std::ranges::move(view, std::back_inserter(result));
    return result;
}

std::array<Queue const*, Renderer::PHASES_SIZE> Renderer::Config::queues() const { return _queues; }

}


//TEMP old code
//VkExtent2D Renderer::minimized() const {
//    VkExtent2D extent = _window->extent();
//    while(extent.width == 0 || extent.height == 0) {
//        extent = _window->extent();
//        glfwWaitEvents();
//    }
//    return extent;
//}


//void Renderer::recreateSwapchain() {
//    VkExtent2D windowExtent = minimized();
//
//    vkDeviceWaitIdle(_core->vkDevice());
//
//    if(_swapchain == nullptr) {
//        _swapchain = std::make_unique<BasicSwapchain>(_core, _deletionQueue, *_window->surface(), windowExtent);
//        return;
//    }
//
//    std::shared_ptr oldSwapchain = std::move(_swapchain);
//    _swapchain = std::make_unique<BasicSwapchain>(_core, _deletionQueue, *_window->surface(), windowExtent, oldSwapchain);
//
//    //TODO i dont understand why the formats cant change?
//    const bool change = oldSwapchain->compareSwapFormats(*_swapchain);
//
//    CTH_STABLE_ERR(change, "depth or image format changed")
//        throw details->exception();
//}

//void Renderer::createSwapchain() {
//    vkDeviceWaitIdle(_core->vkDevice());
//    _swapchain = make_unique<BasicSwapchain>(_core, _deletionQueue, *_window->surface(), _window->extent());
//}
//const PrimaryCmdBuffer* Renderer::beginRender() {
//
//
//    auto buffer = commandBuffer();
//
//
//    const VkResult beginResult = buffer->begin();
//    CTH_STABLE_ERR(beginResult != VK_SUCCESS, "failed to begin command buffer")
//        throw cth::except::vk_result_exception{beginResult, details->exception()};
//
//    return buffer;
//}
//const PrimaryCmdBuffer* Renderer::beginFramea() {
//    CTH_ERR(_frameStarted, "more than one frame started")
//        throw details->exception();
//
//    const VkResult nextImageResult = _swapchain->acquireNextImage(&_currentImageIndex);
//
//    if(nextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
//        recreateSwapchain();
//        return nullptr;
//    }
//
//    CTH_STABLE_ERR(nextImageResult != VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR, "failed to acquire swapchain image")
//        throw cth::except::vk_result_exception{nextImageResult, details->exception()};
//
//    _frameStarted = true;
//
//    auto buffer = commandBuffer();
//
//    const VkResult beginResult = buffer->begin();
//    CTH_STABLE_ERR(beginResult != VK_SUCCESS, "failed to begin command buffer")
//        throw cth::except::vk_result_exception{beginResult, details->exception()};
//
//    return buffer;
//}
//void Renderer::endFramea() {
//    CTH_ERR(!_frameStarted, "no frame active") throw details->exception();
//
//    const auto cmdBuffer = commandBuffer();
//    //TODO finish this up
//    /*   if(device->presentQueueIndex() != device->graphicsQueueIndex())
//           swapchain->changeSwapchainImageQueue(cmdBuffer, device->presentQueueIndex(), currentImageIndex);*/
//
//
//    const VkResult recordResult = cmdBuffer->end();
//
//    CTH_STABLE_ERR(recordResult != VK_SUCCESS, "failed to record command buffer")
//        throw cth::except::vk_result_exception{recordResult, details->exception()};
//
//    const VkResult submitResult = _swapchain->submitCommandBuffer(_deletionQueue, cmdBuffer, _currentImageIndex);
//
//
//    if(submitResult == VK_ERROR_OUT_OF_DATE_KHR || submitResult == VK_SUBOPTIMAL_KHR || _window->windowResized()) {
//        recreateSwapchain();
//        _camera->correctViewRatio(screenRatio());
//        _window->resetWindowResized();
//    } else
//        CTH_STABLE_ERR(submitResult != VK_SUCCESS, "failed to present swapchain image")
//            throw cth::except::vk_result_exception{submitResult, details->exception()};
//
//    _frameStarted = false;
//    ++_currentFrameIndex %= constants::MAX_FRAMES_IN_FLIGHT;
//}



//const PrimaryCmdBuffer* Renderer::beginInitCmdBuffer() {
//    _cmdBuffers.back().emplace_back(make_unique<PrimaryCmdBuffer>(_cmdPools.back().get()));
//    auto& cmdBuffer = _cmdBuffers.back().back();
//    cmdBuffer->begin();
//    return _cmdBuffers.back().back().get();
//}
//void Renderer::endInitBuffer() {
//    auto& cmdBuffer = _cmdBuffers.back().back();
//    cmdBuffer->end();
//
//    vector<VkCommandBuffer> buffers{cmdBuffer->get()};
//    VkSubmitInfo submitInfo{};
//    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//    submitInfo.commandBufferCount = 1;
//    submitInfo.pCommandBuffers = buffers.data();
//
//    vkQueueSubmit(_core->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
//    vkQueueWaitIdle(_core->graphicsQueue());
//
//    _deletionQueue->clear(_currentFrameIndex);
//
//    _cmdBuffers.back().pop_back();
//}
