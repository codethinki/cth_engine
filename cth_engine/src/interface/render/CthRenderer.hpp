#pragma once
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/render/control/CthTimelineSemaphore.hpp"
#include "vulkan/render/control/CthWaitStage.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <mdspan>
#include <memory>
#include <optional>
#include <vector>



namespace cth::vk {
struct BasicGraphicsSyncConfig;
}

namespace cth::vk {
class Queue;
class BasicCore;
class DestructionQueue;
class Device;
class OSWindow;
class CmdPool;
class PrimaryCmdBuffer;
class Camera;


class Renderer {
public:
    struct Config;
    enum Phase : size_t {
        PHASE_TRANSFER,
        PHASE_GRAPHICS,

        PHASES_FIRST = PHASE_TRANSFER,
        PHASES_LAST = PHASE_GRAPHICS,
        PHASES_SIZE
    };

    explicit Renderer(BasicCore const* core, DestructionQueue* destruction_queue, Config const& config);
    ~Renderer();

    /**
     * @brief begins the recording for the phase
     * @tparam P Phase
     * @return cmd_buffer to submit to
     * @throws cth::except::vk_result_exception result of vkBeginCommandBuffer()
     */
    template<Phase P>
    [[nodiscard]] PrimaryCmdBuffer* begin() const;

    /**
     * @brief ends the recording for the phase
     * @tparam P Phase
     * @throws cth::except::vk_result_exception result of vkEndCommandBuffer()
     */
    template<Phase P>
    void end();

    /**
     * @brief used to explicitly skip a phase
     * @tparam P Phase
     */
    template<Phase P>
    void skip();

    /**
     * @brief blocks until the last recorded phase was executed
     * @note there is no need to wait for a skipped phase
     */
    void wait() const;

private:
    template<Phase P> void nextState();


    template<Phase P> void submit();

    void init(Config const& config);
    void createCmdPools();
    void createPrimaryCmdBuffers();
    void createSyncObjects();
    void createSubmitInfos(Config config);

    BasicCore const* _core;
    DestructionQueue* _destructionQueue;

    std::array<Queue const*, PHASES_SIZE> _queues{};
    std::array<std::unique_ptr<PrimaryCmdBuffer>, PHASES_SIZE * constants::FRAMES_IN_FLIGHT> _cmdBuffers;
    std::array<std::unique_ptr<CmdPool>, PHASES_SIZE> _cmdPools;
    std::vector<Queue::SubmitInfo> _submitInfos;

    std::array<size_t, constants::FRAMES_IN_FLIGHT> _stateCounters{};
    std::array<std::unique_ptr<TimelineSemaphore>, constants::FRAMES_IN_FLIGHT> _semaphores{};

    Phase _state = PHASE_TRANSFER;

    uint32_t _frameIndex = 0;


    [[nodiscard]] TimelineSemaphore* semaphore() const { return _semaphores[_frameIndex].get(); }
    template<Phase P> [[nodiscard]] Queue const* queue() const;
    template<Phase P> [[nodiscard]] PrimaryCmdBuffer* cmdBuffer() const;
    template<Phase P> [[nodiscard]] Queue::SubmitInfo& submitInfo() {
        return _submitInfos[P * constants::FRAMES_IN_FLIGHT + _frameIndex]; //TODO change this to frame major instead of phase major
    }


    //[[nodiscard]] size_t to_signal(const State state) const { return _frameStateCounter + state; }
    //void signal(const State state) const { semaphore().signal(to_signal(state)); }
    [[nodiscard]] std::array<PipelineWaitStage, constants::FRAMES_IN_FLIGHT> createWaitSet() const;
    [[nodiscard]] std::array<BasicSemaphore*, constants::FRAMES_IN_FLIGHT> createSignalSet() const;

#ifdef CONSTANT_DEBUG_MODE
    template<Phase P>
    static void debug_check_current_phase(Renderer const* renderer);

    template<Phase P>
    static constexpr void debug_check_phase();
    template<Phase P>
    static void debug_check_phase_change(Renderer const* renderer);
#endif

public:
    [[nodiscard]] uint32_t frameIndex() const { return _frameIndex; }
    [[nodiscard]] DestructionQueue* destructionQueue() const;

    Renderer(Renderer const&) = delete;
    Renderer& operator=(Renderer const&) = delete;


#ifdef CONSTANT_DEBUG_MODE
#define DEBUG_CHECK_RENDERER_CURRENT_PHASE(renderer_ptr, phase) Renderer::debug_check_current_phase<phase>(renderer_ptr)
#define DEBUG_CHECK_RENDERER_PHASE(phase) Renderer::debug_check_phase<phase>()
#define DEBUG_CHECK_RENDERER_PHASE_CHANGE(renderer_ptr, phase) Renderer::debug_check_phase_change<phase>(renderer_ptr)
#else
#define DEBUG_CHECK_RENDERER_CURRENT_PHASE(renderer_ptr, phase) ((void)0)
#define DEBUG_CHECK_RENDERER_PHASE(renderer_ptr, phase) ((void)0)
#define DEBUG_CHECK_RENDERER_PHASE_CHANGE(renderer_ptr, phase) ((void)0)
#endif
};


}

//Builder

namespace cth::vk {

//TEMP left off here. implement this builder. the sync primitives should persist throughout the renderer's lifetime
//TEMP the builder produces the Queue::SubmitInfo objects
//TEMP continue with implementing the graphicsPhase core so the renderer doesnt have to know about the pipelines
//TEMP wait sets have an implicit pipelinestageflag, no need to specify

/**
 * @brief configuration interface for hooking sync primitives to the renderer
 * @note \b set:  collection of sync primitives, one for each frame in constant::FRAMES_IN_FLIGHT
 */
struct Renderer::Config {
    static constexpr size_t SET_SIZE = constants::FRAMES_IN_FLIGHT;

    static Config Render(Queue const* graphics_queue, BasicGraphicsSyncConfig* sync_config);

    Config() = default;
    //Config(const BasicCore* core, DestructionQueue* destruction_queue);

    /**
     * @tparam P phase
     * @note sets.size() % SET_SIZE must be 0
     */
    template<Phase P>
    Config& addSignalSets(std::span<BasicSemaphore* const> signal_semaphore_sets);

    /**
    * @tparam P phase
    * @note sets.size() % SET_SIZE must be 0
    */
    template<Phase P>
    Config& addWaitSets(std::span<PipelineWaitStage const> wait_stage_sets);

    /**
    * @tparam P phase
    * @note sets.size() % SET_SIZE must be 0
    */
    template<Phase P>
    Config& addWaitSets(std::span<BasicSemaphore*> wait_semaphores, VkPipelineStageFlags wait_stage);


    /**
     * @tparam P phase
     * @note sets.size() % SET_SIZE must be 0
     */
    template<Phase P>
    Config& removeSignalSets(std::span<BasicSemaphore* const> signal_semaphore_sets);
    /**
     * @tparam P phase
     * @note sets.size() % SET_SIZE must be 0
     */
    template<Phase P>
    Config& removeWaitSets(std::span<VkPipelineStageFlags const> wait_stage_sets);

    /**
     * @tparam P phase
     * @note every phase needs exactly one queue to proceed
     */
    template<Phase P>
    Config& addQueue(Queue const* queue);


    /**
     * @tparam P phase
     */
    template<Phase P>
    Config& removeQueue(Queue const* queue);

    /**
     * @brief shortcut for addQueue(), addSignalSets() and addWaitSets()
     */
    template<Phase P>
    Config& addPhase(Queue const* queue, std::optional<std::span<BasicSemaphore* const>> signal_semaphore_sets = std::nullopt,
        std::optional<std::span<PipelineWaitStage const>> wait_stage_sets = std::nullopt);


    /**
     *@brief shortcut for addSignalSets() and addWaitSets()
     */
    template<Phase P>
    Config& addSets(std::span<BasicSemaphore* const> signal_semaphore_sets, std::span<PipelineWaitStage const> wait_stage_sets);

private:
    /**
     * @tparam P phase
     * @param phase_buffers cmd_buffers
     * @return vector(SET_SIZE)
     */
    template<Phase P>
    [[nodiscard]] std::vector<Queue::SubmitInfo> createPhaseSubmitInfos(
        std::span<PrimaryCmdBuffer const* const> phase_buffers) const;

    /**
     * @brief creates the SubmitInfos for the Phases and FrameSets
     * @param cmd_buffers span[phase][frame]
     * @return vector[phase][frame] -> SubmitInfo
     */
    [[nodiscard]] std::vector<Queue::SubmitInfo> createSubmitInfos(
        std::span<PrimaryCmdBuffer const* const> cmd_buffers) const;


    template<class T>
    using collection_t = std::array<std::vector<T>, PHASES_SIZE>;

    /**
     * @brief adds the sets to the collection
     * @tparam P phase
     * @param sets size() % SET_SIZE must be 0
     */
    template<Phase P, class T> void add(std::span<T const> sets, collection_t<T>& to);
    /**
     * @brief removes the sets from the collection
     * @tparam P phase
     * @param sets size() % SET_SIZE must be 0
     */
    template<Phase P, class T> void remove(std::span<T const> sets, collection_t<T>& from);

    std::array<Queue const*, PHASES_SIZE> _queues{};
    collection_t<BasicSemaphore*> _phaseSignalSets{};
    collection_t<PipelineWaitStage> _phaseWaitSets{};

    friend class Renderer;

    [[nodiscard]] std::array<Queue const*, PHASES_SIZE> queues() const;

public:
#ifdef CONSTANT_DEBUG_MODE
    template<class Rng>
    static void debug_check_sets_size(Rng const& rng); //asserts: rng.size() % SET_SIZE == 0
#define DEBUG_CHECK_RENDERER_CONFIG_SET_SIZE(rng) Config::debug_check_sets_size(rng)
#else
#define DEBUG_CHECK_RENDERER_CONFIG_SET_SIZE(rng) ((void)0)
#endif



};
}

#include "CthRenderer.inl"



//TEMP old code
///**
// * @throws cth::except::vk_result_exception result of  Swapchain::acquireNextImage()
// * @throws cth::except::vk_result_exception result of vkBeginCommandBuffer()
// */
//const PrimaryCmdBuffer* beginFrame();
///**
// * @throws cth::except::vk_result_exception result of Swapchain::submitCommandBuffers()
// * @throws cth::except::vk_result_exception result of vkEndCommandBuffer()
// */
//void endFrame();
