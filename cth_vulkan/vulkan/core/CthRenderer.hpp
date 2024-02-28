#pragma once

#include "../core/HlcSwapchain.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace cth {
class Camera;

using namespace std;
class Renderer {
public:
    /**
     * \throws cth::except::data_exception data: VkResult of  Swapchain::acquireNextImage()
     * \throws cth::except:data_exception data: VkResult of vkBeginCommandBuffer()
     */
    VkCommandBuffer beginFrame();
    /**
     * \throws cth::except::data_exception data: VkResult of Swapchain::submitCommandBuffers()
     * \throws cth::ecept::data_exception data: VkResult of vkEndCommandBuffer()
     */
    void endFrame();

    void beginSwapchainRenderPass(VkCommandBuffer command_buffer) const;
    void endSwapchainRenderPass(VkCommandBuffer command_buffer) const;

    explicit Renderer(Camera* camera, Device* device);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

private:
    /**
     * \brief waits until not longer minimized
     * \return new window extent
     */
    VkExtent2D minimizedState();

    /**
     * \throws cth::except::data_exception data: VkResult of vkAllocateCommandBuffers()
     */
    void createCommandBuffers();
    void freeCommandBuffers();


    /**
     * \throws cth::except::default_exception reason: depth or image format changed
     */
    void recreateSwapchain();


    Device* device;
    Camera* camera;

    unique_ptr<HlcSwapchain> swapchain;
    vector<VkCommandBuffer> commandBuffers;

    uint32_t currentImageIndex;
    uint_fast8_t currentFrameIndex = 0;
    bool frameStarted = false;

public:
    [[nodiscard]] VkRenderPass swapchainRenderPass() const { return swapchain->getRenderPass(); }
    [[nodiscard]] float screenRatio() const { return swapchain->extentAspectRatio(); }
    [[nodiscard]] bool frameInProgress() const { return frameStarted; }
    [[nodiscard]] VkCommandBuffer commandBuffer() const;
    [[nodiscard]] uint32_t frameIndex() const;
    [[nodiscard]] VkSampleCountFlagBits msaaSampleCount() const { return swapchain->getMsaaSampleCount(); }

};
}
