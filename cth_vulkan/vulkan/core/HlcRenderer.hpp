#pragma once

#include  "CthDevice.hpp"
#include "../core/HlcSwapchain.hpp"
#include "../core/HlcWindow.hpp"
#include "../utils/HlcShader.hpp"


#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace cth {
class Camera;
using namespace std;
class Renderer {
public:
    VkCommandBuffer beginFrame();
    void endFrame();

    void beginSwapchainRenderPass(VkCommandBuffer command_buffer) const;
    void endSwapchainRenderPass(VkCommandBuffer command_buffer) const;

    explicit Renderer(Camera& camera, Device& device);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] VkRenderPass getSwapchainRenderPass() const { return hlcSwapchain->getRenderPass(); }
    [[nodiscard]] float screenRatio() const { return hlcSwapchain->extentAspectRatio(); }

    [[nodiscard]] bool frameInProgress() const { return frameStarted; }
    [[nodiscard]] VkCommandBuffer getCommandBuffer() const {
        assert(frameStarted && "getCommandBuffer: frame didn't start");
        return commandBuffers[currentFrameIndex];
    }

    [[nodiscard]] uint32_t getFrameIndex() const {
        assert(frameStarted && "getFrameIndex: no frame in progress");
        return currentFrameIndex;
    }

    [[nodiscard]] VkSampleCountFlagBits getMsaaSampleCount() const { return hlcSwapchain->getMsaaSampleCount(); }

private:
    void createCommandBuffers();
    void freeCommandBuffers();

    void recreateSwapchain();


    Device& hlcDevice;
    Camera& camera;

    unique_ptr<HlcSwapchain> hlcSwapchain;
    vector<VkCommandBuffer> commandBuffers;

    uint32_t currentImageIndex;
    uint_fast8_t currentFrameIndex = 0;
    bool frameStarted = false;
};
}
