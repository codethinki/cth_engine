#pragma once

#include "CthDevice.hpp"

// vulkan headers
#include <vulkan/vulkan.h>

// std lib headers
#include <memory>
#include <vector>


namespace cth {

class HlcSwapchain {
public:
	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

	HlcSwapchain(Device* device_ref, VkExtent2D window_extent);
	HlcSwapchain(Device* device_ref, VkExtent2D window_extent, std::shared_ptr<HlcSwapchain> previous);
	~HlcSwapchain();

	HlcSwapchain(const HlcSwapchain&) = delete;
	HlcSwapchain& operator=(const HlcSwapchain&) = delete;

	//TODO make this uint32_t
	[[nodiscard]] VkFramebuffer getFrameBuffer(const int index) const { return swapchainFramebuffers[index]; }
	[[nodiscard]] VkRenderPass getRenderPass() const { return renderPass; }
	[[nodiscard]] VkImageView getImageView(const int index) const { return imageViewsSwapchain[index]; }
	[[nodiscard]] size_t imageCount() const { return imagesSwapchain.size(); }
	[[nodiscard]] VkFormat getSwapchainImageFormat() const { return swapchainImageFormat; }
	[[nodiscard]] VkExtent2D getSwapchainExtent() const { return swapchainExtent; }
	[[nodiscard]] uint32_t width() const { return swapchainExtent.width; }
	[[nodiscard]] uint32_t height() const { return swapchainExtent.height; }
	[[nodiscard]] VkImage getSwapchainImage(const int index) const { return imagesSwapchain[index]; }
	[[nodiscard]] VkSampleCountFlagBits getMsaaSampleCount() const { return msaaSamples; }

	static VkImageView createImageView(const VkDevice& device, VkImage image, VkFormat format,
		VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t mip_levels = 1);

	[[nodiscard]] float extentAspectRatio() const { return static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height); }
	VkFormat findDepthFormat() const;

	VkResult acquireNextImage(uint32_t* image_index) const;
	VkResult submitCommandBuffers(const VkCommandBuffer* buffers, const uint32_t* image_index);

	[[nodiscard]] bool compareSwapFormats(const HlcSwapchain& swapchain) const {
		return swapchain.swapchainDepthFormat == swapchainDepthFormat && swapchain.swapchainImageFormat ==
			swapchainImageFormat;
	}

private:
	void init();
	void createSwapchain();
	void createImageViews();
	void createColorResources();
	void createDepthResources();
	void createRenderPass();
	void createFramebuffers();
	void createSyncObjects();

	// Helper functions
	[[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(
		const std::vector<VkSurfaceFormatKHR>& available_formats);
	[[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(
		const std::vector<VkPresentModeKHR>& available_present_modes);
	[[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;


	[[nodiscard]] VkSampleCountFlagBits evaluateMsaaSampleCount() const;
	void setMsaaSampleCount();

	VkFormat swapchainImageFormat;
	VkFormat swapchainDepthFormat;
	VkExtent2D swapchainExtent;

	vector<VkFramebuffer> swapchainFramebuffers;
	VkRenderPass renderPass;

	vector<VkImage> imagesSwapchain;
	vector<VkImageView> imageViewsSwapchain;

	vector<VkImage> imagesDepth;
	vector<VkImageView> imageViewsDepth;
	vector<VkDeviceMemory> imageMemoriesDepth;

	vector<VkImage> imagesMsaa;
	vector<VkImageView> imageViewsMsaa;
	vector<VkDeviceMemory> imageMemoriesMsaa;


	Device& device;
	VkExtent2D windowExtent;

	VkSwapchainKHR swapchain;
	shared_ptr<HlcSwapchain> oldSwapchain; //TODO why is this a shared_ptr?

	vector<VkSemaphore> imageAvailableSemaphores;
	vector<VkSemaphore> renderFinishedSemaphores;
	vector<VkFence> inFlightFences;
	vector<VkFence> imagesInFlight;
	size_t currentFrame = 0;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	static constexpr VkSampleCountFlagBits MAX_MSAA_COUNT = VK_SAMPLE_COUNT_8_BIT;
};

}