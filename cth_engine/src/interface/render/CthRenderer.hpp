#pragma once
#include "vulkan/surface/CthSwapchain.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>



namespace cth {
class Context;
class DeletionQueue;
class Device;
class Window;
class CmdPool;
class PrimaryCmdBuffer;

class Camera;


using namespace std;
class Renderer {
public:
    explicit Renderer(Device* device, DeletionQueue* deletion_queue, Camera* camera, Window* window);
    ~Renderer();

    /**
     * \throws cth::except::vk_result_exception result of  Swapchain::acquireNextImage()
     * \throws cth::except::vk_result_exception result of vkBeginCommandBuffer()
     */
    const PrimaryCmdBuffer* beginFrame();
    /**
     * \throws cth::except::vk_result_exception result of Swapchain::submitCommandBuffers()
     * \throws cth::except::vk_result_exception result of vkEndCommandBuffer()
     */
    void endFrame();

    void beginSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const;
    void endSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const;

    //TEMP move this somewhere else
    [[nodiscard]] const PrimaryCmdBuffer* beginInitCmdBuffer();
    void endInitBuffer();

private:
    /**
     * \brief waits until not longer minimized
     * \return new window extent
     */
    [[nodiscard]] VkExtent2D minimizedState() const;

    /**
    * \throws cth::except::default_exception reason: depth or image format changed
    */
    void recreateSwapchain();

    void init();
    void createSwapchain();
    void createCmdPools();
    void createPrimaryCmdBuffers();

    Device* _device;
    Camera* _camera;
    Window* _window;
    DeletionQueue* _deletionQueue;

    unique_ptr<Swapchain> _swapchain;

    static constexpr size_t PRESENT_QUEUE_I = 0;
    vector<unique_ptr<CmdPool>> _cmdPools;
    vector<vector<unique_ptr<PrimaryCmdBuffer>>> _cmdBuffers;


    uint32_t _currentImageIndex = 0;
    uint_fast8_t _currentFrameIndex = 0;
    bool _frameStarted = false;

public:
    [[nodiscard]] VkRenderPass swapchainRenderPass() const { return _swapchain->renderPass(); }
    [[nodiscard]] float screenRatio() const { return _swapchain->extentAspectRatio(); }
    [[nodiscard]] bool frameInProgress() const { return _frameStarted; }
    [[nodiscard]] uint32_t frameIndex() const;
    [[nodiscard]] VkSampleCountFlagBits msaaSampleCount() const { return _swapchain->msaaSamples(); }
    [[nodiscard]] const PrimaryCmdBuffer* commandBuffer() const;
    [[nodiscard]] DeletionQueue* deletionQueue() const;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
};
}
