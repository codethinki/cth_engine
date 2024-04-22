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
Renderer::Renderer(Device* device, Camera* camera, Window* window) : device{device}, camera{camera}, window(window), currentImageIndex{0} { init(); }
Renderer::~Renderer() {

    cmdBuffers.clear();
    cmdPools.clear();
    swapchain = nullptr;
}

const PrimaryCmdBuffer* Renderer::beginFrame() {
    CTH_ERR(frameStarted, "more than one frame started")
        throw details->exception();

    const VkResult nextImageResult = swapchain->acquireNextImage(&currentImageIndex);

    if(nextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return nullptr;
    }

    CTH_STABLE_ERR(nextImageResult != VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR, "failed to acquire swapchain image")
        throw cth::except::vk_result_exception{nextImageResult, details->exception()};

    frameStarted = true;

    auto buffer = commandBuffer();

    const VkResult beginResult = buffer->begin();
    CTH_STABLE_ERR(beginResult != VK_SUCCESS, "failed to begin command buffer")
        throw cth::except::vk_result_exception{beginResult, details->exception()};

    return buffer;
}
void Renderer::endFrame() {
    CTH_ERR(!frameStarted, "no frame active") throw details->exception();

    const auto cmdBuffer = commandBuffer();
    //TODO finish this up
    /*   if(device->presentQueueIndex() != device->graphicsQueueIndex())
           swapchain->changeSwapchainImageQueue(cmdBuffer, device->presentQueueIndex(), currentImageIndex);*/


    const VkResult recordResult = cmdBuffer->end();

    CTH_STABLE_ERR(recordResult != VK_SUCCESS, "failed to record command buffer")
        throw cth::except::vk_result_exception{recordResult, details->exception()};

    const VkResult submitResult = swapchain->submitCommandBuffer(_deletionQueue.get(), cmdBuffer, currentImageIndex);


    if(submitResult == VK_ERROR_OUT_OF_DATE_KHR || submitResult == VK_SUBOPTIMAL_KHR || window->windowResized()) {
        recreateSwapchain();
        camera->correctViewRatio(screenRatio());
        window->resetWindowResized();
    } else
        CTH_STABLE_ERR(submitResult != VK_SUCCESS, "failed to present swapchain image")
            throw cth::except::vk_result_exception{submitResult, details->exception()};

    frameStarted = false;
    ++currentFrameIndex %= MAX_FRAMES_IN_FLIGHT;
}

void Renderer::beginSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const {
    CTH_ERR(!frameStarted, "no frame started") throw details->exception();
    CTH_ERR(command_buffer != commandBuffer(), "renderPass already started")
        throw details->exception();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = swapchain->renderPass();
    renderPassInfo.framebuffer = swapchain->framebuffer(currentImageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain->extent();

    array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0, 0, 0, 1};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(command_buffer->get(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(swapchain->extent().width);
    viewport.height = static_cast<float>(swapchain->extent().height);
    viewport.minDepth = 0;
    viewport.maxDepth = 1.0f;
    const VkRect2D scissor{{0, 0}, swapchain->extent()};
    vkCmdSetViewport(command_buffer->get(), 0, 1, &viewport);
    vkCmdSetScissor(command_buffer->get(), 0, 1, &scissor);

}
void Renderer::endSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const {
    CTH_ERR(!frameStarted, "no frame active") throw details->exception();
    CTH_ERR(command_buffer != commandBuffer(), "only one command buffer allowed")
        throw details->exception();

    vkCmdEndRenderPass(command_buffer->get());
}
const PrimaryCmdBuffer* Renderer::beginInitCmdBuffer() {
    cmdBuffers.back().emplace_back(make_unique<PrimaryCmdBuffer>(cmdPools.back().get()));
    auto& cmdBuffer = cmdBuffers.back().back();
    cmdBuffer->begin();
    return cmdBuffers.back().back().get();
}
void Renderer::endInitBuffer() {
    auto& cmdBuffer = cmdBuffers.back().back();
    cmdBuffer->end();

    vector<VkCommandBuffer> buffers{cmdBuffer->get()};
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = buffers.data();

    vkQueueSubmit(device->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(device->graphicsQueue());

    _deletionQueue->clear(currentFrameIndex);

    cmdBuffers.back().pop_back();
}



VkExtent2D Renderer::minimizedState() const {
    VkExtent2D extent = window->extent();
    while(extent.width == 0 || extent.height == 0) {
        extent = window->extent();
        glfwWaitEvents();
    }
    return extent;
}


void Renderer::recreateSwapchain() {
    VkExtent2D windowExtent = minimizedState();

    vkDeviceWaitIdle(device->get());

    if(swapchain == nullptr) {
        swapchain = make_unique<Swapchain>(device, _deletionQueue.get(), windowExtent, window->surface());
        return;
    }

    shared_ptr oldSwapchain = std::move(swapchain);
    swapchain = make_unique<Swapchain>(device, _deletionQueue.get(), windowExtent, window->surface(), oldSwapchain);

    //TODO i dont understand why the formats cant change?
    const bool change = oldSwapchain->compareSwapFormats(*swapchain);

    CTH_STABLE_ERR(change, "depth or image format changed")
        throw details->exception();
}

void Renderer::init() {
    createDeletionQueue();
    createSwapchain();
    createCmdPools();
    createPrimaryCmdBuffers();
}
void Renderer::createSwapchain() {
    vkDeviceWaitIdle(device->get());
    swapchain = make_unique<Swapchain>(device, _deletionQueue.get(), window->extent(), window->surface());
}
void Renderer::createCmdPools() {
    for(auto index : device->uniqueQueueIndices())
        cmdPools.emplace_back(make_unique<CmdPool>(device, CmdPool::Config::Default(index, MAX_FRAMES_IN_FLIGHT + 1, 0)));
}
void Renderer::createPrimaryCmdBuffers() {
    cmdBuffers.resize(cmdPools.size());
    for(auto [cmdPool, buffers] : views::zip(cmdPools, cmdBuffers)) {
        buffers.reserve(MAX_FRAMES_IN_FLIGHT + 1);
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
            buffers.emplace_back(make_unique<PrimaryCmdBuffer>(cmdPool.get()));
    }
}
void Renderer::createDeletionQueue() { _deletionQueue = make_unique<DeletionQueue>(device); }

uint32_t Renderer::frameIndex() const {
    CTH_ERR(!frameStarted, "no frame active") throw details->exception();
    return currentFrameIndex;
}
const PrimaryCmdBuffer* Renderer::commandBuffer() const {
    //must be here bc of compile error without #include "cmd/CthCmdBuffer.hpp"
    CTH_ERR(!frameStarted, "no frame active") throw details->exception();
    return cmdBuffers.back()[currentFrameIndex].get();
}
DeletionQueue* Renderer::deletionQueue() const { return _deletionQueue.get(); }



} // namespace cth
