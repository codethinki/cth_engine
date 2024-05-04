#include "CthRenderer.hpp"

#include "interface/user/HlcCamera.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/surface/CthWindow.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <array>

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/cmd/CthCmdPool.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"



namespace cth {
Renderer::Renderer(Device* device, DeletionQueue* deletion_queue, Camera* camera, Window* window) : _device{device}, _deletionQueue(deletion_queue), _camera{camera}, _window(window), _currentImageIndex{0} { init(); }
Renderer::~Renderer() {

    _cmdBuffers.clear();
    _cmdPools.clear();
    _swapchain = nullptr;
}

const PrimaryCmdBuffer* Renderer::beginFrame() {
    CTH_ERR(_frameStarted, "more than one frame started")
        throw details->exception();

    const VkResult nextImageResult = _swapchain->acquireNextImage(&_currentImageIndex);

    if(nextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return nullptr;
    }

    CTH_STABLE_ERR(nextImageResult != VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR, "failed to acquire swapchain image")
        throw cth::except::vk_result_exception{nextImageResult, details->exception()};

    _frameStarted = true;

    auto buffer = commandBuffer();

    const VkResult beginResult = buffer->begin();
    CTH_STABLE_ERR(beginResult != VK_SUCCESS, "failed to begin command buffer")
        throw cth::except::vk_result_exception{beginResult, details->exception()};

    return buffer;
}
void Renderer::endFrame() {
    CTH_ERR(!_frameStarted, "no frame active") throw details->exception();

    const auto cmdBuffer = commandBuffer();
    //TODO finish this up
    /*   if(device->presentQueueIndex() != device->graphicsQueueIndex())
           swapchain->changeSwapchainImageQueue(cmdBuffer, device->presentQueueIndex(), currentImageIndex);*/


    const VkResult recordResult = cmdBuffer->end();

    CTH_STABLE_ERR(recordResult != VK_SUCCESS, "failed to record command buffer")
        throw cth::except::vk_result_exception{recordResult, details->exception()};

    const VkResult submitResult = _swapchain->submitCommandBuffer(_deletionQueue, cmdBuffer, _currentImageIndex);


    if(submitResult == VK_ERROR_OUT_OF_DATE_KHR || submitResult == VK_SUBOPTIMAL_KHR || _window->windowResized()) {
        recreateSwapchain();
        _camera->correctViewRatio(screenRatio());
        _window->resetWindowResized();
    } else
        CTH_STABLE_ERR(submitResult != VK_SUCCESS, "failed to present swapchain image")
            throw cth::except::vk_result_exception{submitResult, details->exception()};

    _frameStarted = false;
    ++_currentFrameIndex %= Constant::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::beginSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const {
    CTH_ERR(!_frameStarted, "no frame started") throw details->exception();
    CTH_ERR(command_buffer != commandBuffer(), "renderPass already started")
        throw details->exception();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _swapchain->renderPass();
    renderPassInfo.framebuffer = _swapchain->framebuffer(_currentImageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = _swapchain->extent();

    array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0, 0, 0, 1};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(command_buffer->get(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(_swapchain->extent().width);
    viewport.height = static_cast<float>(_swapchain->extent().height);
    viewport.minDepth = 0;
    viewport.maxDepth = 1.0f;
    const VkRect2D scissor{{0, 0}, _swapchain->extent()};
    vkCmdSetViewport(command_buffer->get(), 0, 1, &viewport);
    vkCmdSetScissor(command_buffer->get(), 0, 1, &scissor);

}
void Renderer::endSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const {
    CTH_ERR(!_frameStarted, "no frame active") throw details->exception();
    CTH_ERR(command_buffer != commandBuffer(), "only one command buffer allowed")
        throw details->exception();

    vkCmdEndRenderPass(command_buffer->get());
}
const PrimaryCmdBuffer* Renderer::beginInitCmdBuffer() {
    _cmdBuffers.back().emplace_back(make_unique<PrimaryCmdBuffer>(_cmdPools.back().get()));
    auto& cmdBuffer = _cmdBuffers.back().back();
    cmdBuffer->begin();
    return _cmdBuffers.back().back().get();
}
void Renderer::endInitBuffer() {
    auto& cmdBuffer = _cmdBuffers.back().back();
    cmdBuffer->end();

    vector<VkCommandBuffer> buffers{cmdBuffer->get()};
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = buffers.data();

    vkQueueSubmit(_device->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_device->graphicsQueue());

    _deletionQueue->clear(_currentFrameIndex);

    _cmdBuffers.back().pop_back();
}



VkExtent2D Renderer::minimizedState() const {
    VkExtent2D extent = _window->extent();
    while(extent.width == 0 || extent.height == 0) {
        extent = _window->extent();
        glfwWaitEvents();
    }
    return extent;
}


void Renderer::recreateSwapchain() {
    VkExtent2D windowExtent = minimizedState();

    vkDeviceWaitIdle(_device->get());

    if(_swapchain == nullptr) {
        _swapchain = make_unique<Swapchain>(_device, _deletionQueue, windowExtent, _window->surface());
        return;
    }

    shared_ptr oldSwapchain = std::move(_swapchain);
    _swapchain = make_unique<Swapchain>(_device, _deletionQueue, windowExtent, _window->surface(), oldSwapchain);

    //TODO i dont understand why the formats cant change?
    const bool change = oldSwapchain->compareSwapFormats(*_swapchain);

    CTH_STABLE_ERR(change, "depth or image format changed")
        throw details->exception();
}

void Renderer::init() {
    createSwapchain();
    createCmdPools();
    createPrimaryCmdBuffers();
}
void Renderer::createSwapchain() {
    vkDeviceWaitIdle(_device->get());
    _swapchain = make_unique<Swapchain>(_device, _deletionQueue, _window->extent(), _window->surface());
}
void Renderer::createCmdPools() {
    for(auto index : _device->uniqueQueueIndices())
        _cmdPools.emplace_back(make_unique<CmdPool>(_device, CmdPool::Config::Default(index, Constant::MAX_FRAMES_IN_FLIGHT + 1, 0)));
}
void Renderer::createPrimaryCmdBuffers() {
    _cmdBuffers.resize(_cmdPools.size());
    for(auto [cmdPool, buffers] : views::zip(_cmdPools, _cmdBuffers)) {
        buffers.reserve(Constant::MAX_FRAMES_IN_FLIGHT + 1);
        for(size_t i = 0; i < Constant::MAX_FRAMES_IN_FLIGHT; ++i)
            buffers.emplace_back(make_unique<PrimaryCmdBuffer>(cmdPool.get()));
    }
}


uint32_t Renderer::frameIndex() const {
    CTH_ERR(!_frameStarted, "no frame active") throw details->exception();
    return _currentFrameIndex;
}
const PrimaryCmdBuffer* Renderer::commandBuffer() const {
    //must be here bc of compile error without #include "cmd/CthCmdBuffer.hpp"
    CTH_ERR(!_frameStarted, "no frame active") throw details->exception();
    return _cmdBuffers.back()[_currentFrameIndex].get();
}
DeletionQueue* Renderer::deletionQueue() const { return _deletionQueue; }



} // namespace cth
