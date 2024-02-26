#include "HlcSwapchain.hpp"

// std
#include <array>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>


namespace cth {

HlcSwapchain::HlcSwapchain(Device& device_ref, const VkExtent2D window_extent) : device{device_ref}, windowExtent{window_extent} { init(); }
HlcSwapchain::HlcSwapchain(Device& device_ref, const VkExtent2D window_extent, std::shared_ptr<HlcSwapchain> previous) : device{device_ref},
	windowExtent{window_extent},
	oldSwapchain{std::move(previous)} {
	init();
	oldSwapchain = nullptr;
}
void HlcSwapchain::init() {
	createSwapchain();
	createImageViews();

	setMsaaSampleCount();
	createRenderPass();
	createColorResources();
	createDepthResources();

	createFramebuffers();
	createSyncObjects();
}


HlcSwapchain::~HlcSwapchain() {
	for(const auto imageView : imageViewsSwapchain) { vkDestroyImageView(device.device(), imageView, nullptr); }
	imageViewsSwapchain.clear();

	if(swapchain != nullptr) {
		vkDestroySwapchainKHR(device.device(), swapchain, nullptr);
		swapchain = nullptr;
	}

	for(int i = 0; i < imagesDepth.size(); i++) {
		vkDestroyImageView(device.device(), imageViewsDepth[i], nullptr);
		vkDestroyImage(device.device(), imagesDepth[i], nullptr);
		vkFreeMemory(device.device(), imageMemoriesDepth[i], nullptr);
	}

	for(int i = 0; i < imagesMsaa.size(); i++) {
		vkDestroyImageView(device.device(), imageViewsMsaa[i], nullptr);
		vkDestroyImage(device.device(), imagesMsaa[i], nullptr);
		vkFreeMemory(device.device(), imageMemoriesMsaa[i], nullptr);
	}

	for(const auto framebuffer : swapchainFramebuffers) { vkDestroyFramebuffer(device.device(), framebuffer, nullptr); }

	vkDestroyRenderPass(device.device(), renderPass, nullptr);

	// cleanup synchronization objects
	for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device.device(), renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device.device(), imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device.device(), inFlightFences[i], nullptr);
	}
}

VkResult HlcSwapchain::acquireNextImage(uint32_t* image_index) const {
	vkWaitForFences(
		device.device(),
		1,
		&inFlightFences[currentFrame],
		VK_TRUE,
		std::numeric_limits<uint64_t>::max());

	const VkResult result = vkAcquireNextImageKHR(
		device.device(),
		swapchain,
		std::numeric_limits<uint64_t>::max(),
		imageAvailableSemaphores[currentFrame],
		// must be a not signaled semaphore
		VK_NULL_HANDLE,
		image_index);

	return result;
}

VkResult HlcSwapchain::submitCommandBuffers(const VkCommandBuffer* buffers, const uint32_t* image_index) {
	if(imagesInFlight[*image_index] != VK_NULL_HANDLE) vkWaitForFences(device.device(), 1, &imagesInFlight[*image_index], VK_TRUE, UINT64_MAX);
	imagesInFlight[*image_index] = inFlightFences[currentFrame];

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	const VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
	const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = buffers;

	const VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device.device(), 1, &inFlightFences[currentFrame]);
	if(vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
		throw std::runtime_error(
			"failed to submit draw command buffer!");

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	const VkSwapchainKHR swapchains[] = {swapchain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;

	presentInfo.pImageIndices = image_index;

	const auto result = vkQueuePresentKHR(device.getPresentQueue(), &presentInfo);

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

	return result;
}

void HlcSwapchain::createSwapchain() {
	const SwapchainSupportDetails swapchainSupport = device.getSwapchainSupport();

	const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
	const VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
	const VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

	uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
	if(swapchainSupport.capabilities.maxImageCount > 0 &&
		imageCount > swapchainSupport.capabilities.maxImageCount)
		imageCount = swapchainSupport.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = device.surface();

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const QueueFamilyIndices indices = device.findPhysicalQueueFamilies();
	const uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

	if(indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = oldSwapchain == nullptr ? VK_NULL_HANDLE : oldSwapchain->swapchain;

	if(vkCreateSwapchainKHR(device.device(), &createInfo, nullptr, &swapchain) != VK_SUCCESS)
		throw
			std::runtime_error("failed to create swap chain!");


	// we only specified a minimum number of images in the swap chain, so the implementation is
	// allowed to create a swap chain with more. That's why we'll first query the final number of
	// images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
	// retrieve the handles.
	vkGetSwapchainImagesKHR(device.device(), swapchain, &imageCount, nullptr);
	imagesSwapchain.resize(imageCount);
	vkGetSwapchainImagesKHR(device.device(), swapchain, &imageCount, imagesSwapchain.data());

	swapchainImageFormat = surfaceFormat.format;
	swapchainExtent = extent;
}

VkImageView HlcSwapchain::createImageView(const VkDevice& device, const VkImage image, const VkFormat format, const VkImageAspectFlags aspect_flags,
	const uint32_t mip_levels) {
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspect_flags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mip_levels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	VkImageView imageView;
	if(vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) throw runtime_error("createImageView: failed to create image view");
	return imageView;
}

void HlcSwapchain::createImageViews() {
	imageViewsSwapchain.resize(imagesSwapchain.size());
	for(size_t i = 0; i < imagesSwapchain.size(); i++) imageViewsSwapchain[i] = createImageView(
		device.device(), 
		imagesSwapchain[i],
		swapchainImageFormat
	);
}

void HlcSwapchain::createColorResources() {
	cout << "Msaa sample count: " << msaaSamples << '\n';

	imagesMsaa.resize(imageCount());
	imageMemoriesMsaa.resize(imageCount());
	imageViewsMsaa.resize(imageCount());


	for(int i = 0; i < imagesMsaa.size(); i++) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = swapchainExtent.width;
		imageInfo.extent.height = swapchainExtent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = swapchainImageFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.samples = msaaSamples;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.flags = 0;
		//BUG read access violation :(
		device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imagesMsaa[i], imageMemoriesMsaa[i]);

		imageViewsMsaa[i] = createImageView(device.device(), imagesMsaa[i], swapchainImageFormat);
	}
}

void HlcSwapchain::createDepthResources() {
	const VkFormat depthFormat = findDepthFormat();
	swapchainImageFormat = depthFormat;

	imagesDepth.resize(imageCount());
	imageMemoriesDepth.resize(imageCount());
	imageViewsDepth.resize(imageCount());


	for(int i = 0; i < imagesDepth.size(); i++) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = swapchainExtent.width;
		imageInfo.extent.height = swapchainExtent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = depthFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageInfo.samples = msaaSamples;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.flags = 0;

		device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			imagesDepth[i], imageMemoriesDepth[i]);

		imageViewsDepth[i] = createImageView(device.device(), imagesDepth[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	}
}

void HlcSwapchain::createRenderPass() {
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = getSwapchainImageFormat();
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = msaaSamples;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


	VkAttachmentDescription colorAttachmentResolve{};
	colorAttachmentResolve.format = swapchainImageFormat;
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


	VkAttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depthAttachmentRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	VkAttachmentReference colorAttachmentResolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	subpass.pResolveAttachments = &colorAttachmentResolveRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.srcAccessMask = 0;
	dependency.srcStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstSubpass = 0;

	dependency.dstStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask =
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if(vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) throw runtime_error("failed to create render pass!");
}

void HlcSwapchain::createFramebuffers() {
	swapchainFramebuffers.resize(imageCount());

	for(size_t i = 0; i < imageCount(); i++) {
		array<VkImageView, 3> attachments = { imageViewsMsaa[i], imageViewsDepth[i], imageViewsSwapchain[i]};

		const VkExtent2D swapchainExtent = getSwapchainExtent();
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapchainExtent.width;
		framebufferInfo.height = swapchainExtent.height;
		framebufferInfo.layers = 1;

		if(vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr,
			&swapchainFramebuffers[i]) != VK_SUCCESS)
			throw runtime_error("failed to create framebuffer!");
	}
}

void HlcSwapchain::createSyncObjects() {
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if(vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) !=
			VK_SUCCESS ||
			vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) !=
			VK_SUCCESS ||
			vkCreateFence(device.device(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}
}

VkSurfaceFormatKHR HlcSwapchain::chooseSwapSurfaceFormat(
	const std::vector<VkSurfaceFormatKHR>& available_formats) {
	for(const auto& availableFormat : available_formats) {
		if(availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
			availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { return availableFormat; }
	}

	return available_formats[0];
}

VkPresentModeKHR HlcSwapchain::chooseSwapPresentMode(
	const std::vector<VkPresentModeKHR>& available_present_modes) {
	for(const auto& availablePresentMode : available_present_modes) {
		if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			cout << "Present mode: Mailbox" << std::endl;
			return availablePresentMode;
		}
	}

	// for (const auto &availablePresentMode : availablePresentModes) {
	//   if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
	//     std::cout << "Present mode: Immediate" << std::endl;
	//     return availablePresentMode;
	//   }
	// }

	cout << "Present mode: V-Sync" << std::endl;
	return VK_PRESENT_MODE_FIFO_KHR;
	//return VK_PRESENT_MODE_IMMEDIATE_KHR;
}

VkExtent2D HlcSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
	if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) { return capabilities.currentExtent; } else {
		VkExtent2D actualExtent = windowExtent;
		actualExtent.width = std::max(
			capabilities.minImageExtent.width,
			std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(
			capabilities.minImageExtent.height,
			std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

VkSampleCountFlagBits HlcSwapchain::evaluateMsaaSampleCount() const {
	const uint32_t maxSamples = device.evaluateMaxUsableSampleCount();

	bool found = false;
	uint32_t samples = 1;
	while(samples < maxSamples && samples < MAX_MSAA_COUNT) samples *= 2;

	return static_cast<VkSampleCountFlagBits>(samples);
}
void HlcSwapchain::setMsaaSampleCount() { msaaSamples = evaluateMsaaSampleCount(); }


VkFormat HlcSwapchain::findDepthFormat() const {
	return device.findSupportedFormat(
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

}