#include "CthRenderer.hpp"

#include "interface/user/HlcCamera.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/cmd/CthCmdPool.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/surface/CthOSWindow.hpp"

#include <cth/cth_log.hpp>

#include <array>

//TEMP cleanup this file


namespace cth {


Renderer::Renderer(Camera* camera, OSWindow* window, const Config& config) : _core{config._core},
    _deletionQueue(config._deletionQueue), _queues(config.queues()), _camera{camera}, _window(window) { init(config); }
Renderer::~Renderer() {

    std::ranges::fill(_cmdBuffers, nullptr);
    std::ranges::fill(_cmdPools, nullptr);
}


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


VkExtent2D Renderer::minimized() const {
    VkExtent2D extent = _window->extent();
    while(extent.width == 0 || extent.height == 0) {
        extent = _window->extent();
        glfwWaitEvents();
    }
    return extent;
}


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



void Renderer::init(const Config& config) {
    createCmdPools();
    createPrimaryCmdBuffers();
    createSyncObjects();

    createSubmitInfos(config);
}
//void Renderer::createSwapchain() {
//    vkDeviceWaitIdle(_core->vkDevice());
//    _swapchain = make_unique<BasicSwapchain>(_core, _deletionQueue, *_window->surface(), _window->extent());
//}
void Renderer::createCmdPools() {
    for(size_t i = PHASE_FIRST; i < PHASE_LAST; ++i)
        _cmdPools[i] = std::make_unique<CmdPool>(_core, CmdPool::Config::Default(_queues[i]->familyIndex(), Constant::FRAMES_IN_FLIGHT + 1, 0));
}
void Renderer::createPrimaryCmdBuffers() {
    for(size_t i = PHASE_FIRST; i < PHASE_LAST; ++i)
        for(size_t j = 0; j < Constant::FRAMES_IN_FLIGHT; ++j)
            _cmdBuffers[i * PHASES_SIZE + j] = std::make_unique<PrimaryCmdBuffer>(_cmdPools[i].get());
}
void Renderer::createSyncObjects() {
    std::ranges::transform(_semaphores, _semaphores.begin(),
        [this](auto&) { return std::make_unique<TimelineSemaphore>(_core, _deletionQueue); }
        );
}
void Renderer::createSubmitInfos(Config config) {

    config.addWaitSets<PHASE_TRANSFER>()

    auto cmdBuffers = _cmdBuffers
        | std::views::transform([](const unique_ptr<PrimaryCmdBuffer>& buffer) { return buffer.get(); })
        | std::ranges::to<std::array<const PrimaryCmdBuffer*, PHASES_SIZE * Constant::FRAMES_IN_FLIGHT>>();

    _submitInfos = config.createSubmitInfos(cmdBuffers);
}



uint32_t Renderer::frameIndex() const {
    CTH_ERR(!_frameActive, "no frame active") throw details->exception();
    return _frameIndex;
}

DeletionQueue* Renderer::deletionQueue() const { return _deletionQueue; }


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
//    ++_currentFrameIndex %= Constant::MAX_FRAMES_IN_FLIGHT;
//}


//TEMP this needs to be in the graphics core
//void Renderer::beginSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const {
//    CTH_ERR(!_transferStarted, "no frame started") throw details->exception();
//    CTH_ERR(command_buffer != commandBuffer(), "renderPass already started")
//        throw details->exception();
//
//    VkRenderPassBeginInfo renderPassInfo{};
//    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//    renderPassInfo.renderPass = _swapchain->renderPass();
//    renderPassInfo.framebuffer = _swapchain->framebuffer(_currentImageIndex);
//    renderPassInfo.renderArea.offset = {0, 0};
//    renderPassInfo.renderArea.extent = _swapchain->extent();
//
//    array<VkClearValue, 2> clearValues{};
//    clearValues[0].color = {0, 0, 0, 1};
//    clearValues[1].depthStencil = {1.0f, 0};
//    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
//    renderPassInfo.pClearValues = clearValues.data();
//
//    vkCmdBeginRenderPass(command_buffer->get(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
//
//    VkViewport viewport;
//    viewport.x = 0;
//    viewport.y = 0;
//    viewport.width = static_cast<float>(_swapchain->extent().width);
//    viewport.height = static_cast<float>(_swapchain->extent().height);
//    viewport.minDepth = 0;
//    viewport.maxDepth = 1.0f;
//    const VkRect2D scissor{{0, 0}, _swapchain->extent()};
//    vkCmdSetViewport(command_buffer->get(), 0, 1, &viewport);
//    vkCmdSetScissor(command_buffer->get(), 0, 1, &scissor);
//
//}
//void Renderer::endSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const {
//    CTH_ERR(!_transferStarted, "no frame active") throw details->exception();
//    CTH_ERR(command_buffer != commandBuffer(), "only one command buffer allowed")
//        throw details->exception();
//
//    vkCmdEndRenderPass(command_buffer->get());
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



} // namespace cth

//Builder

namespace cth {
Renderer::Config::Config(const BasicCore* core, DeletionQueue* deletion_queue) : _core{core}, _deletionQueue{deletion_queue} {
    DEBUG_CHECK_CORE(core);
    DEBUG_CHECK_DELETION_QUEUE(deletion_queue);
}



auto Renderer::Config::createSubmitInfos(const std::span<const PrimaryCmdBuffer* const> cmd_buffers) const
    -> std::array<Queue::SubmitInfo, SET_SIZE * PHASES_SIZE> {
    DEBUG_CHECK_RENDERER_CONFIG_SET_SIZE(cmd_buffers);

    auto phaseBuffers = cmd_buffers | std::views::chunk(SET_SIZE);

    std::array phaseSubmitInfos = {
        createPhaseSubmitInfos<PHASE_TRANSFER>(phaseBuffers[PHASE_TRANSFER]),
        createPhaseSubmitInfos<PHASE_RENDER>(phaseBuffers[PHASE_RENDER]),
    };

    auto views = phaseSubmitInfos | std::views::join;
    std::array<Queue::SubmitInfo, SET_SIZE * PHASES_SIZE> submitInfos{};
    std::ranges::copy(views, submitInfos.begin());
    return submitInfos;
}

std::array<const Queue*, Renderer::PHASES_SIZE> Renderer::Config::queues() const {
    for(auto& queue : _queues)
        DEBUG_CHECK_QUEUE(queue);

    return _queues;
}

}
