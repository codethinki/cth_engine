#include "HlcRenderer.hpp"

#include "../user/HlcCamera.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>


namespace cth {
void Renderer::recreateSwapchain() {
	auto extent = Window::getExtent();
	while(extent.width == 0 || extent.height == 0) {
		extent = Window::getExtent();
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(hlcDevice.device());

	if(hlcSwapchain == nullptr) hlcSwapchain = make_unique<HlcSwapchain>(hlcDevice, extent);
	else {
		shared_ptr oldSwapchain = move(hlcSwapchain);
		hlcSwapchain = make_unique<HlcSwapchain>(hlcDevice, extent, oldSwapchain);
		if(!oldSwapchain->compareSwapFormats(*hlcSwapchain.get()))
			throw runtime_error(
				"recreateSwapchain: depth or image format changed");
	}

}

void Renderer::createCommandBuffers() {
	commandBuffers.resize(hlcSwapchain->MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = hlcDevice.getCommandPool();
	allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	if(vkAllocateCommandBuffers(hlcDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) throw runtime_error(
		"createCommandBuffers: failed to create command buffers");
};
void Renderer::freeCommandBuffers() {
	vkFreeCommandBuffers(hlcDevice.device(), hlcDevice.getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	commandBuffers.clear();
}

VkCommandBuffer Renderer::beginFrame() {
	assert(!frameStarted && "beginFrame: can't begin 2 Frames at the same time");

	const auto result = hlcSwapchain->acquireNextImage(&currentImageIndex);

	if(result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain();
		return nullptr;
	}
	if(result != VK_SUCCESS && result !=
		VK_SUBOPTIMAL_KHR)
		throw runtime_error("begin Frame: failed to acquire swapchain image");

	frameStarted = true;

	const auto commandBuffer = getCommandBuffer();
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) throw runtime_error("beginFrame: failed to begin recording command buffer");

	return commandBuffer;
}
void Renderer::endFrame() {
	assert(frameStarted && "endFrame: can't end a non existing frame");
	const auto commandBuffer = getCommandBuffer();
	if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) throw runtime_error("endFrame: failed to record command buffer");

	const auto result = hlcSwapchain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
	if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || Window::windowResized()) {
		Window::resetWindowResized();
		recreateSwapchain();

		camera.correctViewRatio(screenRatio());
	}
	else if(result != VK_SUCCESS) throw runtime_error("endFrame: failed to present swapchain image");

	frameStarted = false;
	++currentFrameIndex %= HlcSwapchain::MAX_FRAMES_IN_FLIGHT;

}

void Renderer::beginSwapchainRenderPass(VkCommandBuffer command_buffer) const {
	assert(frameStarted && "beginSwapchainRenderPass: can't write to non existing frame");
	assert(command_buffer == getCommandBuffer() && "beginSwapchainRenderPass: Can't begin two render passes at once");
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = hlcSwapchain->getRenderPass();
	renderPassInfo.framebuffer = hlcSwapchain->getFrameBuffer(static_cast<int>(currentImageIndex));
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = hlcSwapchain->getSwapchainExtent();

	array<VkClearValue, 2> clearValues{};
	clearValues[0].color = {0, 0, 0, 1};
	clearValues[1].depthStencil = {1.0f, 0};
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = static_cast<float>(hlcSwapchain->getSwapchainExtent().width);
	viewport.height = static_cast<float>(hlcSwapchain->getSwapchainExtent().height);
	viewport.minDepth = 0;
	viewport.maxDepth = 1.0f;
	const VkRect2D scissor{{0, 0}, hlcSwapchain->getSwapchainExtent()};
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

}
void Renderer::endSwapchainRenderPass(VkCommandBuffer command_buffer) const {
	assert(frameStarted && "endRenderPass: can't end a non existing frame");
	assert(command_buffer == getCommandBuffer() && "endRenderPass: Can't end other render passes");
	vkCmdEndRenderPass(command_buffer);
}

Renderer::Renderer(Camera& camera, Device& device) : hlcDevice{device}, camera{camera}, currentImageIndex{0} {
	recreateSwapchain();
	createCommandBuffers();
}
Renderer::~Renderer() { freeCommandBuffers(); }
}
