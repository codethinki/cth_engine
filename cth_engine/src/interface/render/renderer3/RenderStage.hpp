#pragma once
#include "src/vulkan/render/control/CthPipelineWaitStage.hpp"
#include "src/vulkan/utility/cth_constants.hpp"



namespace cth::vk {
class CmdPool;
}

namespace cth::vk {
class Renderer3;
}

namespace cth::vk {
class Queue;
struct PipelineWaitStage;
class Semaphore;

struct RenderStageConfig {
    static constexpr auto GROUP_SIZE = constants::FRAMES_IN_FLIGHT;
    using id_t = size_t;


    cth::not_null<Queue const*> queue;
    /**
     * @attention must match the @ref queue family
     */
    cth::not_null<CmdPool*> cmdPool;
    std::vector<Semaphore*> signalSemaphores{};
    std::vector<PipelineWaitStage> waitStages{};
};
}

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

class RenderStage {
public:
    using Config = RenderStageConfig;

    RenderStage(Core const& core, Config config);

    /**
     * @brief create constructor
     * @details calls: @ref create()
     */
    RenderStage(Core const& core, Config config, create_t);

    /**
     * @details calls @ref optDestroy()
     */
    ~RenderStage();

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
    std::vector<Semaphore*> _signalSemaphores;
    std::vector<PipelineWaitStage> _waitStages;

    VkExtent2D _extent{};
    std::vector<SubmitInfo> _submitInfos;
    std::vector<PrimaryCmdBuffer> _cmdBuffers;


    size_t _subIndex = 0;

    [[nodiscard]] size_t subIndex() const { return _subIndex; }
    [[nodiscard]] PrimaryCmdBuffer& cmdBuffer();
    [[nodiscard]] SubmitInfo& submitInfo();

    [[nodiscard]] auto const& current(auto const& rng) const { return rng[_subIndex]; }
    [[nodiscard]] auto& current(auto& rng) { return rng[_subIndex]; }

public:
    [[nodiscard]] bool created() const { return !_signalSemaphores.empty() && !_waitStages.empty(); }
    [[nodiscard]] bool recording() const;

    RenderStage(RenderStage const& other) = delete;
    RenderStage& operator=(RenderStage const& other) = delete;
    RenderStage(RenderStage&& other) noexcept = default;
    RenderStage& operator=(RenderStage&& other) noexcept = default;
};

}
