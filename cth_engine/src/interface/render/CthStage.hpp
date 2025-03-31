#pragma once
#include "src/vulkan/render/control/CthPipelineWaitStage.hpp"
#include "src/vulkan/utility/cth_constants.hpp"

#include <volk.h>

#include <cth/pointer/not_null.hpp>


namespace cth::vk {
struct Cycle;
struct PipelineWaitStage;
struct SubmitInfo;

class CmdPool;
class Core;
class PrimaryCmdBuffer;
class Queue;
class RenderPass;
class Semaphore;

class Stage {
public:
    struct Config;
    /**
     * @brief create constructor
     * @details calls: @ref create()
     */
    Stage(Core const& core, Config config);
    /**
     * @details calls @ref optDestroy()
     */
    ~Stage();

    /**
     * @details calls: @ref optDestroy()
     */
    void create();
    /**
     * @brief destroys the objects state
     */
    void destroy();

    /**
     * @brief destroys if created
     * @details calls if @ref created() @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }


    /**
     * @brief calls @ref PrimaryCmdBuffer::begin()
     * @return cmd buffer in recording state
     */
    PrimaryCmdBuffer* begin();

    /**
     * @brief calls @ref PrimaryCmdBuffer::end()
     */
    void end();

    /**
     * @brief submits the stage to the queue
     * @attention requires @ref recording() == true or @ref end() called
     * @note calls @ref Queue::submit(SubmitInfo const&);
     */
    void submit();


    /**
     * @brief skip submits the stage
     * @note triggers sync primitives
     */
    void skip();

private:
    /**
     * @details calls @ref CmdPool::CmdPool(Core const&, CmdPool::Config const&, bool);
     */
    void createCmdPool();

    /**
     * @details calls @ref PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool*, VkCommandBufferUsageFlags);
     */
    void createCmdBuffers();

    void createSubmitInfos();



    cth::not_null<Core const*> _core;


    cth::not_null<Queue const*> _queue;
    std::unique_ptr<RenderPass> _renderPass;
    std::vector<Semaphore*> _signalSemaphores;
    std::vector<PipelineWaitStage> _waitStages;

    VkExtent2D _extent{};
    std::vector<SubmitInfo> _submitInfos;
    std::unique_ptr<CmdPool> _cmdPool;
    std::vector<PrimaryCmdBuffer> _cmdBuffers;


    friend class Renderer;
    size_t _subIndex = 0;

    [[nodiscard]] size_t subIndex() const { return _subIndex; }
    [[nodiscard]] PrimaryCmdBuffer& cmdBuffer();
    [[nodiscard]] SubmitInfo& submitInfo();

    [[nodiscard]] auto const& current(auto const& rng) const { return rng[_subIndex]; }
    [[nodiscard]] auto& current(auto& rng) { return rng[_subIndex]; }

public:
    [[nodiscard]] bool created() const { return _renderPass != nullptr && !_signalSemaphores.empty() && !_waitStages.empty(); }
    [[nodiscard]] bool recording() const;
};

}


//Config

namespace cth::vk {
struct Stage::Config {
    static constexpr auto GROUP_SIZE = constants::FRAMES_IN_FLIGHT;


    /**
     * @brief default constructor
     * @param queue queue to render to
     * @param render_pass pass to render in
     * @param signal_groups optional signal groups to add
     * @param wait_groups optional wait groups to add
     */
    explicit Config(Queue const& queue, std::unique_ptr<RenderPass> render_pass, std::span<Semaphore* const> signal_groups = {},
        std::span<PipelineWaitStage const> wait_groups = {});
    ~Config();


    /**
     * @brief adds a group of semaphores to signal once the stage finished rendering
     * @param self self
     * @param signal_group group of semaphores (size == @ref GROUP_SIZE and created required)
     * @return self
     */
    Config& addSignalGroup(this Config& self, std::span<Semaphore* const> signal_group);

    /**
     * @brief adds a group of semaphores to wait on before rendering the stage
     * @param self self
     * @param wait_group group of semaphores to wait on (size == @ref GROUP_SIZE and created required) 
     * @return self
     */
    Config& addWaitGroup(this Config& self, std::span<PipelineWaitStage const> wait_group);

    /**
     * @brief adds multiple signal groups
     * @param self self
     * @param signal_groups n signal groups to add
     * @return self
     * @note calls @ref addSignalGroup(this Config& self, std::span<Semaphore* const>)
     */
    Config& addSignalGroups(this Config& self, std::span<Semaphore* const> signal_groups);
    /**
     * @brief adds multiple signal groups
     * @param self self
     * @param wait_groups n wait groups to add
     * @return self
     * @note calls @ref addWaitGroup(this Config& self, std::span<PipelineWaitStage const>)
     */
    Config& addWaitGroups(this Config& self, std::span<PipelineWaitStage const> wait_groups);

private:
    cth::not_null<Queue const*> _queue;
    std::unique_ptr<RenderPass> _renderPass;
    std::vector<Semaphore*> _signalSemaphores{};
    std::vector<PipelineWaitStage> _waitStages{};

    friend class Stage;
};
}
