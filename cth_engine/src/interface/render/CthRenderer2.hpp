#pragma once
#pragma once
#include "CthRenderCycle.hpp"
#include "CthStage.hpp"
#include "Phase.hpp"

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
class Stage;
class GraphicsSyncConfig;
class Queue;
class Core;
class DestructionQueue;
class Device;
class OSWindow;
class CmdPool;
class PrimaryCmdBuffer;
class Camera;


class Renderer2 {
public:
    struct Config;
    static constexpr size_t PHASES_SIZE = 2;

    explicit Renderer2(cth::not_null<Core const*> core, std::span<Phase::Config> phases);
    ~Renderer2();

    /**
     * @brief begins recording for phases
     * @return phases cmd buffers
     * @throws cth::vk::result_exception result of @ref vkBeginCommandBuffer()
     */
    [[nodiscard]] std::vector<Stage*> begin() const;

    /**
     * @brief ends the recording for the phase
     * @throws cth::vk::result_exception result of @ref vkEndCommandBuffer()
     */
    void end();

    /**
     * @brief used to explicitly skip a phase
     * @tparam P Phase
     */
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
    void nextState();


    void submit();

    void init(Config const& config);
    void createCmdPools();
    void createPrimaryCmdBuffers();
    void createSyncObjects();
    void createSubmitInfos(Config config);

    cth::not_null<Core const*> _core;


    std::vector<Phase> _phases;

    std::array<std::unique_ptr<TimelineSemaphore>, constants::FRAMES_IN_FLIGHT> _semaphores{};

    size_t _state = 0;

    Cycle _cycle{};


    //[[nodiscard]] size_t to_signal(const State state) const { return _frameStateCounter + state; }
    //void signal(const State state) const { semaphore().signal(to_signal(state)); }
    [[nodiscard]] std::array<PipelineWaitStage, constants::FRAMES_IN_FLIGHT> createWaitSet() const;
    [[nodiscard]] std::array<Semaphore*, constants::FRAMES_IN_FLIGHT> createSignalSet() const;

    static void debug_check_current_phase(Renderer2 const* Renderer2);

    static constexpr void debug_check_phase();
    static void debug_check_phase_change(Renderer2 const* Renderer2);

public:
    Renderer2(Renderer2 const&) = delete;
    Renderer2& operator=(Renderer2 const&) = delete;
};


}