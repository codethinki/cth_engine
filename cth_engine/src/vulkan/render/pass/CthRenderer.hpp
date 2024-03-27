#pragma once
#include "vulkan/surface/CthSwapchain.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>


namespace cth {
class Device;
class Window;
class Camera;

using namespace std;
class Renderer {
public:
    explicit Renderer(Device* device, Camera* camera, Window* window);
    ~Renderer();

    /**
     * \throws cth::except::vk_result_exception result of  Swapchain::acquireNextImage()
     * \throws cth::except::vk_result_exception result of vkBeginCommandBuffer()
     */
    VkCommandBuffer beginFrame();
    /**
     * \throws cth::except::vk_result_exception result of Swapchain::submitCommandBuffers()
     * \throws cth::except::vk_result_exception result of vkEndCommandBuffer()
     */
    void endFrame();

    void beginSwapchainRenderPass(VkCommandBuffer command_buffer) const;
    void endSwapchainRenderPass(VkCommandBuffer command_buffer) const;

private:
    /**
     * \brief waits until not longer minimized
     * \return new window extent
     */
    VkExtent2D minimizedState() const;

    /**
     * \throws cth::except::vk_result_exception result of vkAllocateCommandBuffers()
     */
    void createCommandBuffers();
    void freeCommandBuffers();


    /**
     * \throws cth::except::default_exception reason: depth or image format changed
     */
    void recreateSwapchain();


    Device* device;
    Camera* camera;
    Window* window;

    unique_ptr<Swapchain> swapchain;
    vector<VkCommandBuffer> commandBuffers;

    uint32_t currentImageIndex = 0;
    uint_fast8_t currentFrameIndex = 0;
    bool frameStarted = false;

public:
    [[nodiscard]] VkRenderPass swapchainRenderPass() const { return swapchain->getRenderPass(); }
    [[nodiscard]] float screenRatio() const { return swapchain->extentAspectRatio(); }
    [[nodiscard]] bool frameInProgress() const { return frameStarted; }
    [[nodiscard]] VkCommandBuffer commandBuffer() const;
    [[nodiscard]] uint32_t frameIndex() const;
    [[nodiscard]] VkSampleCountFlagBits msaaSampleCount() const { return swapchain->getMsaaSampleCount(); }

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
};
}
