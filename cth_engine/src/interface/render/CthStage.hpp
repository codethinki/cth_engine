#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include <volk.h>


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
    Stage(cth::not_null<Core*> core, Config config);

    void create();
    void destroy();



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
     * @note calls @ref CmdPool::CmdPool(cth::not_null<Core const*>, CmdPool::Config const&, bool);
     */
    void createCmdPool();

    /**
     * @note calls @ref PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool*, VkCommandBufferUsageFlags);
     */
    void createCmdBuffers();

    void createSubmitInfos();



    cth::not_null<Core*> _core;


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
    [[nodiscard]] bool recording() const;
};

}


//Config

namespace cth::vk {
struct Stage::Config {
    static constexpr auto GROUP_SIZE = constants::FRAMES_IN_FLIGHT;


    explicit Config(cth::not_null<Queue const*> queue, std::unique_ptr<RenderPass> render_pass, std::span<Semaphore* const> signal_groups = {},
        std::span<PipelineWaitStage> wait_groups = {});


    Config& addSignalGroup(this Config& self, std::span<Semaphore* const> signal_group);
    Config& addWaitGroup(this Config& self, std::span<PipelineWaitStage const> wait_group);

    Config& addSignalGroups(this Config& self, std::span<Semaphore* const> signal_groups);
    Config& addWaitGroups(this Config& self, std::span<PipelineWaitStage> wait_groups);

private:
    cth::not_null<Queue const*> _queue;
    std::unique_ptr<RenderPass> _renderPass;
    std::vector<Semaphore*> _signalSemaphores{};
    std::vector<PipelineWaitStage> _waitStages{};

    friend class Stage;
};

}
