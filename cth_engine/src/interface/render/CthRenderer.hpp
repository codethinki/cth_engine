#pragma once
#include "vulkan/render/control/CthTimelineSemaphore.hpp"
#include "vulkan/surface/CthBasicSwapchain.hpp"


#include <vulkan/vulkan.h>

#include <cstdint>
#include <mdspan>
#include <memory>
#include <vector>


namespace cth {
class Queue;
class BasicCore;
class DeletionQueue;
class Device;
class OSWindow;
class CmdPool;
class PrimaryCmdBuffer;
class Camera;


class Renderer {
public:
    explicit Renderer(const BasicCore* core, DeletionQueue* deletion_queue, const Queue* queue, Camera* camera, OSWindow* window);
    ~Renderer();
    ///**
    // * \throws cth::except::vk_result_exception result of  Swapchain::acquireNextImage()
    // * \throws cth::except::vk_result_exception result of vkBeginCommandBuffer()
    // */
    //const PrimaryCmdBuffer* beginFrame();
    ///**
    // * \throws cth::except::vk_result_exception result of Swapchain::submitCommandBuffers()
    // * \throws cth::except::vk_result_exception result of vkEndCommandBuffer()
    // */
    //void endFrame();

    /**
     * \brief requests a new frame
     * \return index of the new frame
     * \note this operation might block
     */
    enum Phase : size_t {
        PHASE_TRANSFER,
        PHASE_RENDER,
        PHASE_MAX_ENUM
    };

    template<Phase P>
    [[nodiscard]] PrimaryCmdBuffer* begin();

    template<Phase P>
    void end();

    template<Phase P>
    void skip();

    size_t beginFrame();

    const PrimaryCmdBuffer* beginTransfer();
    void endTransfer(const PrimaryCmdBuffer* transfer_cmd_buffer);

    void skipTransfer() const;

    const PrimaryCmdBuffer* beginRender();
    void endRender();

    void endFrame();


    void beginSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const;
    void endSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const;

    //TEMP move this somewhere else
    [[nodiscard]] const PrimaryCmdBuffer* beginInitCmdBuffer();
    void endInitBuffer();

private:
    enum State {
        STATE_TRANSFER_READY,
        STATE_RENDER_READY,
        STATE_FINISHED,
    };
    /**
     * \brief waits until not longer minimized
     * \return new window extent
     */
    [[nodiscard]] VkExtent2D minimizedState() const;

    /**
    * \throws cth::except::default_exception reason: depth or image format changed
    */
    void recreateSwapchain();

    template<Phase P> void submit();

    void init();
    void createSwapchain();
    void createCmdPools();
    void createPrimaryCmdBuffers();
    void createSyncObjects();

    const BasicCore* _core;
    DeletionQueue* _deletionQueue;

    std::array<const Queue*, PHASE_MAX_ENUM> _queues;
    std::array<const PrimaryCmdBuffer*, PHASE_MAX_ENUM * Constant::MAX_FRAMES_IN_FLIGHT> _cmdBuffers;
    std::array<CmdPool, PHASE_MAX_ENUM> _cmdPools;

    std::array<size_t, Constant::MAX_FRAMES_IN_FLIGHT> _stateCounters{};
    std::array<TimelineSemaphore, Constant::MAX_FRAMES_IN_FLIGHT> _semaphores;

    State _state = STATE_TRANSFER_READY;

    Camera* _camera;
    OSWindow* _window;



    std::array<size_t, Constant::MAX_FRAMES_IN_FLIGHT> _syncTimelineValues;

    uint32_t _currentImageIndex = 0;
    uint_fast8_t _currentFrameIndex = 0;



    void waitNextFrame() const;

    [[nodiscard]] const TimelineSemaphore& semaphore() const { return _semaphores[_currentFrameIndex]; }
    template<Phase P> [[nodiscard]] const Queue* queue() const;
    template<Phase P> [[nodiscard]] const PrimaryCmdBuffer* cmdBuffer() const;


    //[[nodiscard]] size_t to_signal(const State state) const { return _frameStateCounter + state; }
    //void signal(const State state) const { semaphore().signal(to_signal(state)); }

#ifdef CONSTANT_DEBUG_MODE
    template<Phase P>
    static void debug_check_phase(const Renderer* renderer);
#endif

public:
    [[nodiscard]] VkRenderPass swapchainRenderPass() const { return _swapchain->renderPass(); }
    [[nodiscard]] float screenRatio() const { return _swapchain->extentAspectRatio(); }
    [[nodiscard]] uint32_t frameIndex() const;
    [[nodiscard]] VkSampleCountFlagBits msaaSampleCount() const { return _swapchain->msaaSamples(); }
    [[nodiscard]] const PrimaryCmdBuffer* commandBuffer() const;
    [[nodiscard]] DeletionQueue* deletionQueue() const;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;


#ifdef CONSTANT_DEBUG_MODE
#define DEBUG_CHECK_RENDERER_PHASE(renderer_ptr, phase) Renderer::debug_check_phase<phase>(renderer_ptr);
#else
#define DEBUG_CHECK_RENDERER_PHASE(renderer_ptr, phase) ((void)0)
#endif
};


}

#include "CthRenderer.inl"
