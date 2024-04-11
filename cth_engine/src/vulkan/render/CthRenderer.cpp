#include "CthRenderer.hpp"

#include "interface/user/HlcCamera.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/surface/CthWindow.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <array>



namespace cth {
Renderer::Renderer(Device* device, Camera* camera, Window* window) : device{device}, camera{camera}, window(window), currentImageIndex{0} {
    recreateSwapchain();
    createCommandBuffers();
}
Renderer::~Renderer() { freeCommandBuffers(); }

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
        swapchain = make_unique<Swapchain>(device, windowExtent, window->surface());
        return;
    }

    shared_ptr oldSwapchain = std::move(swapchain);
    swapchain = make_unique<Swapchain>(device, windowExtent, window->surface(), oldSwapchain);

    //TODO i dont understand why the formats cant change?
    const bool change = oldSwapchain->compareSwapFormats(*swapchain);

    CTH_STABLE_ERR(change, "depth or image format changed")
        throw details->exception();
}
VkCommandBuffer Renderer::commandBuffer() const {
    CTH_ERR(!frameStarted, "no frame active") throw details->exception();
    return commandBuffers[currentFrameIndex];
}
uint32_t Renderer::frameIndex() const {
    CTH_ERR(!frameStarted, "no frame active") throw details->exception();
    return currentFrameIndex;
}

void Renderer::createCommandBuffers() {
    commandBuffers.resize(swapchain->MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device->getCommandPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    const VkResult allocResult = vkAllocateCommandBuffers(device->get(), &allocInfo, commandBuffers.data());

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "Vk: failed to allocate command buffers")
        throw cth::except::vk_result_exception{allocResult, details->exception()};
};
void Renderer::freeCommandBuffers() {
    vkFreeCommandBuffers(device->get(), device->getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    commandBuffers.clear();
}

VkCommandBuffer Renderer::beginFrame() {
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

    const auto buffer = commandBuffer();
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    const VkResult beginResult = vkBeginCommandBuffer(buffer, &beginInfo);
    CTH_STABLE_ERR(beginResult != VK_SUCCESS, "failed to begin command buffer")
        throw cth::except::vk_result_exception{beginResult, details->exception()};

    return buffer;
}
void Renderer::endFrame() {
    CTH_ERR(!frameStarted, "no frame active") throw details->exception();

    const auto buffer = commandBuffer();
    const VkResult recordResult = vkEndCommandBuffer(buffer);

    CTH_STABLE_ERR(recordResult != VK_SUCCESS, "failed to record command buffer")
        throw cth::except::vk_result_exception{recordResult, details->exception()};


    const VkResult submitResult = swapchain->submitCommandBuffer(buffer, currentImageIndex);

    if(submitResult == VK_ERROR_OUT_OF_DATE_KHR || submitResult == VK_SUBOPTIMAL_KHR || window->windowResized()) {
        recreateSwapchain();
        camera->correctViewRatio(screenRatio());
        window->resetWindowResized();
    } else
        CTH_STABLE_ERR(submitResult != VK_SUCCESS, "failed to present swapchain image")
            throw cth::except::vk_result_exception{submitResult, details->exception()};

    frameStarted = false;
    ++currentFrameIndex %= Swapchain::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::beginSwapchainRenderPass(VkCommandBuffer command_buffer) const {
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

    vkCmdBeginRenderPass(command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(swapchain->extent().width);
    viewport.height = static_cast<float>(swapchain->extent().height);
    viewport.minDepth = 0;
    viewport.maxDepth = 1.0f;
    const VkRect2D scissor{{0, 0}, swapchain->extent()};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

}
void Renderer::endSwapchainRenderPass(VkCommandBuffer command_buffer) const {
    CTH_ERR(!frameStarted, "no frame active") throw details->exception();
    CTH_ERR(command_buffer != commandBuffer(), "only one command buffer allowed")
        throw details->exception();

    vkCmdEndRenderPass(command_buffer);
}
} // namespace cth
