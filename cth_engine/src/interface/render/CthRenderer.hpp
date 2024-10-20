#pragma once
#include "CthRenderCycle.hpp"

#include "vulkan/base/queue/CthQueue.hpp"
#include "vulkan/base/queue/CthSubmitInfo.hpp"
#include "vulkan/render/control/CthTimelineSemaphore.hpp"
#include "vulkan/render/control/CthWaitStage.hpp"

#include <volk.h>

#include <array>
#include <memory>
#include <optional>
#include <vector>


namespace cth::vk {
class GraphicsSyncConfig;
class Queue;
class Core;
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

    explicit Renderer(cth::not_null<Core const*> core, Config const& config);
    ~Renderer();

    /**
     * @brief begins the recording for the phase
     * @tparam P Phase
     * @return cmd_buffer to submit to
     * @throws cth::vk::result_exception result of @ref vkBeginCommandBuffer()
     */
    template<Phase P>
    [[nodiscard]] PrimaryCmdBuffer* begin() const;

    /**
     * @brief ends the recording for the phase
     * @tparam P Phase
     * @throws cth::vk::result_exception result of @ref vkEndCommandBuffer()
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
     * @brief must be called at the beginning of each cycle
     * @return cycle struct with info for current cycle
     */
    Cycle cycle();

    /**
     * @brief blocks until the last recorded phase with the same cycle index was executed
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

    cth::not_null<Core const*> _core;

    std::array<Queue const*, PHASES_SIZE> _queues{};
    std::array<std::unique_ptr<PrimaryCmdBuffer>, PHASES_SIZE * constants::FRAMES_IN_FLIGHT> _cmdBuffers;
    std::array<std::unique_ptr<CmdPool>, PHASES_SIZE> _cmdPools;
    std::vector<SubmitInfo> _submitInfos;

    std::array<size_t, constants::FRAMES_IN_FLIGHT> _stateCounters{};
    std::array<std::unique_ptr<TimelineSemaphore>, constants::FRAMES_IN_FLIGHT> _semaphores{};

    Phase _state = PHASE_TRANSFER;

    Cycle _cycle{};

    [[nodiscard]] TimelineSemaphore* semaphore() const { return _semaphores[_cycle.subIndex].get(); }
    template<Phase P> [[nodiscard]] Queue const* queue() const;
    template<Phase P> [[nodiscard]] PrimaryCmdBuffer* cmdBuffer() const;
    template<Phase P> [[nodiscard]] SubmitInfo& submitInfo() { return _submitInfos[P * constants::FRAMES_IN_FLIGHT + _cycle.subIndex]; }


    //[[nodiscard]] size_t to_signal(const State state) const { return _frameStateCounter + state; }
    //void signal(const State state) const { semaphore().signal(to_signal(state)); }
    [[nodiscard]] std::array<PipelineWaitStage, constants::FRAMES_IN_FLIGHT> createWaitSet() const;
    [[nodiscard]] std::array<Semaphore*, constants::FRAMES_IN_FLIGHT> createSignalSet() const;

#ifdef CONSTANT_DEBUG_MODE
    template<Phase P>
    static void debug_check_current_phase(Renderer const* renderer);

    template<Phase P>
    static constexpr void debug_check_phase();
    template<Phase P>
    static void debug_check_phase_change(Renderer const* renderer);
#endif

public:
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
/**
 * @brief configuration interface for hooking sync primitives to the renderer
 * @note \b set:  collection of sync primitives, one for each frame in constant::FRAMES_IN_FLIGHT
 */
struct Renderer::Config {
    static constexpr size_t SET_SIZE = constants::FRAMES_IN_FLIGHT;

    static Config Render(Queue const* graphics_queue, GraphicsSyncConfig const* sync_config);

    Config() = default;
    //Config(const Core* core, DestructionQueue* destruction_queue);

    /**
     * @tparam P phase
     * @note sets.size() % SET_SIZE must be 0
     */
    template<Phase P>
    Config& addSignalSets(std::span<Semaphore* const> signal_semaphore_sets);

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
    Config& addWaitSets(std::span<Semaphore* const> wait_semaphores, VkPipelineStageFlags wait_stage);


    /**
     * @tparam P phase
     * @note sets.size() % SET_SIZE must be 0
     */
    template<Phase P>
    Config& removeSignalSets(std::span<Semaphore* const> signal_semaphore_sets);
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
    Config& addPhase(Queue const* queue, std::optional<std::span<Semaphore* const>> signal_semaphore_sets = std::nullopt,
        std::optional<std::span<PipelineWaitStage const>> wait_stage_sets = std::nullopt);


    /**
     *@brief shortcut for addSignalSets() and addWaitSets()
     */
    template<Phase P>
    Config& addSets(std::span<Semaphore* const> signal_semaphore_sets, std::span<PipelineWaitStage const> wait_stage_sets);

private:
    /**
     * @tparam P phase
     * @param phase_buffers cmd_buffers
     * @return vector(SET_SIZE)
     */
    template<Phase P>
    [[nodiscard]] std::vector<SubmitInfo> createPhaseSubmitInfos(
        std::span<PrimaryCmdBuffer const* const> phase_buffers) const;

    /**
     * @brief creates the SubmitInfos for the Phases and FrameSets
     * @param cmd_buffers span[phase][frame]
     * @return vector[phase][frame] -> SubmitInfo
     */
    [[nodiscard]] std::vector<SubmitInfo> createSubmitInfos(
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
    collection_t<Semaphore*> _phaseSignalSets{};
    collection_t<PipelineWaitStage> _phaseWaitSets{};

    friend class Renderer;

    [[nodiscard]] std::array<Queue const*, PHASES_SIZE> queues() const;

public:
    template<class Rng>
    static void debug_check_sets_size(Rng const& rng); //asserts: rng.size() % SET_SIZE == 0
};
}


#include "CthRenderer.inl"


namespace cth::vk {
template<class Rng>
void Renderer::Config::debug_check_sets_size(Rng const& rng) {
    CTH_CRITICAL((std::ranges::size(rng) % SET_SIZE) != 0, "size must be a multiple of SET_SIZE") {}
}
}
