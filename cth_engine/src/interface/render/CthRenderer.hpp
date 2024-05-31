#pragma once
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/render/control/CthTimelineSemaphore.hpp"
#include "vulkan/render/control/CthWaitStage.hpp"

#include <vulkan/vulkan.h>

#include <array>
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
    struct Config;

    explicit Renderer(Camera* camera, OSWindow* window, const Config& builder);
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
    size_t beginFrame();

    template<Phase P>
    [[nodiscard]] PrimaryCmdBuffer* begin();

    template<Phase P>
    void end();

    template<Phase P>
    void skip();

    void endFrame();



    void beginSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const;
    void endSwapchainRenderPass(const PrimaryCmdBuffer* command_buffer) const;

    //TEMP move this somewhere else
    [[nodiscard]] const PrimaryCmdBuffer* beginInitCmdBuffer();
    void endInitBuffer();

private:
    static constexpr Phase PHASE_FIRST = PHASE_TRANSFER;
    static constexpr Phase PHASE_LAST = PHASE_RENDER;
    template<Phase P> void nextState();

    /**
     * \brief waits until not longer minimized
     * \return new window extent
     */
    [[nodiscard]] VkExtent2D minimized() const;

    /**
    * \throws cth::except::default_exception reason: depth or image format changed
    */
    void recreateSwapchain();

    template<Phase P> void submit();

    void init();
    void createCmdPools();
    void createPrimaryCmdBuffers();
    void createSyncObjects();

    const BasicCore* _core;
    DeletionQueue* _deletionQueue;

    std::array<const Queue*, PHASE_MAX_ENUM> _queues;
    std::array<std::unique_ptr<PrimaryCmdBuffer>, PHASE_MAX_ENUM * Constant::MAX_FRAMES_IN_FLIGHT> _cmdBuffers;
    std::array<std::unique_ptr<CmdPool>, PHASE_MAX_ENUM> _cmdPools;

    std::array<size_t, Constant::MAX_FRAMES_IN_FLIGHT> _stateCounters{};
    std::array<TimelineSemaphore, Constant::MAX_FRAMES_IN_FLIGHT> _semaphores;

    Phase _state = PHASE_TRANSFER;

    Camera* _camera;
    OSWindow* _window;


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
    static constexpr void debug_check_phase();
    template<Phase P>
    static void debug_check_phase_change(const Renderer* renderer);
#endif

public:
    [[nodiscard]] uint32_t frameIndex() const;
    [[nodiscard]] const PrimaryCmdBuffer* commandBuffer() const;
    [[nodiscard]] DeletionQueue* deletionQueue() const;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;


#ifdef CONSTANT_DEBUG_MODE
#define DEBUG_CHECK_RENDERER_PHASE(phase ) Renderer::debug_check_phase<phase>();
#define DEBUG_CHECK_RENDERER_PHASE_CHANGE(renderer_ptr, phase) Renderer::debug_check_phase_change<phase>(renderer_ptr);
#else
#define DEBUG_CHECK_RENDERER_PHASE(renderer_ptr, phase) ((void)0)
#endif
};


}

//Builder

namespace cth {

//TEMP left off here. implement this builder. the sync primitives should persist throughout the renderer's lifetime
//TEMP the builder produces the Queue::SubmitInfo objects
//TEMP continue with implementing the graphics core so the renderer doesnt have to know about the pipelines

struct Renderer::Config {
    static constexpr size_t SET_SIZE = Constant::MAX_FRAMES_IN_FLIGHT;

    Config(const BasicCore* core, DeletionQueue* deletion_queue);
    template<Phase P>
    Config& addSignalSet(std::span<BasicSemaphore* const, SET_SIZE> signal_semaphore_set);
    template<Phase P>
    Config& addWaitSet(std::span<const PipelineWaitStage, SET_SIZE> wait_stage_set);

    template<Phase P>
    Config& removeSignalSet(std::span<BasicSemaphore* const, SET_SIZE> signal_semaphores);
    template<Phase P>
    Config& removeWaitSet(std::span<const VkPipelineStageFlags> wait_stages);

    template<Phase P>
    Config& addQueue(const Queue* queue);

    template<Phase P>
    Config& removeQueue(const Queue* queue);

    /**
     * \brief 
     * \tparam P phase to add to
     * \param signal_semaphore_sets sets of signal semaphores
     * \param wait_stage_set sets of wait stages
     * \note set size must be equal to SET_SIZE
     */
    template<Phase P>
    Config& addPhase(const Queue* queue, std::optional<std::span<BasicSemaphore* const>> signal_semaphore_sets = std::nullopt,
        std::optional<std::span<const PipelineWaitStage>> wait_stage_set = std::nullopt);

private:
    template<Phase P>
    std::array<Queue::SubmitInfo, SET_SIZE> createPhaseSubmitInfo(std::span<const PrimaryCmdBuffer* const, 2> cmd_buffers);

    /**
     * \brief creates the SubmitInfos for the Phases and FrameSets
     * \param cmd_buffers span[phase][frame]
     * \return array[phase][frame] -> SubmitInfo
     */
    std::array<Queue::SubmitInfo, SET_SIZE * PHASE_MAX_ENUM> createSubmitInfo(
        std::span<const PrimaryCmdBuffer* const, SET_SIZE * PHASE_MAX_ENUM> cmd_buffers);


    template<class T>
    using collection_t = std::array<std::vector<std::array<T, SET_SIZE>>, PHASE_MAX_ENUM>;

    template<Phase P, class T> void add(std::span<T const, SET_SIZE> signal_semaphores, collection_t<T>& to);
    template<Phase P, class T> void remove(std::span<T const, SET_SIZE> signal_semaphores, collection_t<T>& to);

    const BasicCore* _core;
    DeletionQueue* _deletionQueue;

    std::array<const Queue*, PHASE_MAX_ENUM> _queues;
    collection_t<BasicSemaphore*> _signalSets;
    collection_t<PipelineWaitStage> _waitSets;

    friend class Renderer;

public:
#ifdef CONSTANT_DEBUG_MODE

#else
#endif



};
}

#include "CthRenderer.inl"
