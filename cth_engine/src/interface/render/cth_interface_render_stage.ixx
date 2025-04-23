module;

export module cth.interface.render.stage;

import cth.vk.constants;
import cth.vk.render.sync.fence;
import cth.vk.render.sync.pipeline_wait_stage;
import cth.interface.render.stage_config;

import cth.ptr.not_null;

import std;

namespace cth::vk {
class RenderPulse;

struct Cycle;
struct PipelineWaitStage;
struct SubmitInfo;

class CmdPool;
class Core;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;

class Queue;
class RenderPass;
class Semaphore;
}

//IMPLEMENT release and state
namespace cth::vk {


struct RenderStageCmdBuffers {
    PrimaryCmdBuffer* cmdBuffer;
    std::vector<SecondaryCmdBuffer*> secondaryCmdBuffers;
};

class RenderStage {
public:
    cxpr static uint32_t GROUP_SIZE = constants::FRAMES_IN_FLIGHT;

    using Config = RenderStageConfig;
    RenderStage(Core const& core, RenderPulse const& pulse, Config config);

    /**
     * @brief create constructor
     * @details calls: @ref create()
     */
    RenderStage(Core const& core, RenderPulse const& pulse, Config config, create_t);

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
     * @brief begins this stage and begins the primary cmd buffer, may block
     * @details calls:
            - @ref wait() const
            - @ref PrimaryCmdBuffer::begin()
     * @return cmd buffers
     */
    [[nodiscard]] RenderStageCmdBuffers begin();

    /**
     * @brief calls @ref PrimaryCmdBuffer::end()
     * @attention requires @ref recording()
     */
    void end();
    void optEnd() { if(recording()) end(); }

    /**
     * @brief submits the stage to the queue
     * @attention does NOT call @ref wait() const
     * @details calls:
        - @ref optEnd() 
        - @ref Fence::reset() const
        - @ref Queue::submit(SubmitInfo const&)
     */
    void submit();



    /**
     * @brief skip submits the stage, may block
     * @details calls:
            - @ref wait() const;
            - @ref Fence::reset() const
            - @ref Queue::skip(SubmitInfo&) const
     * @note advances sync primitives
     */
    void skip();

    /**
     * @brief waits for stage completion or times out
     * @param timeout in nanoseconds
     * @return @ref Fence::wait(size_t) const
     */
    [[nodiscard]] VkResult wait(size_t timeout) const;

    /**
     * @brief waits for stage completion
     * @details calls @ref Fence::wait() const
     */
    void wait() const;

private:
    void reset() const;

    void initFences();
    void initCmdPools();
    void initCmdBuffers();
    void initSubmitInfos();

    void init();

    void createFences();
    void createCmdPools();
    void createPrimaryCmdBuffers();
    void createSecondaryCmdBuffers();

    /**
     * @details calls:
        - @ref PrimaryCmdBuffer::create(CmdPool&);
        - @ref SecondaryCmdBuffer::create(CmdPool&);
     */
    void createCmdBuffers();

    void createSubmitInfos();

    cth::not_null<Core const*> _core;
    cth::not_null<RenderPulse const*> _pulse;

    Config _config;

    std::vector<Fence> _fences;
    std::vector<CmdPool> _cmdPools;
    std::vector<PrimaryCmdBuffer> _primaryCmdBuffers;
    std::vector<SecondaryCmdBuffer> _secondaryCmdBuffers;
    std::vector<SubmitInfo> _submitInfos;



    [[nodiscard]] size_t secondaryChunkSize() const;
    [[nodiscard]] PrimaryCmdBuffer& primaryCmdBuffer();
    [[nodiscard]] PrimaryCmdBuffer const& primaryCmdBuffer() const;
    [[nodiscard]] std::vector<SecondaryCmdBuffer*> secondaryCmdBuffers();



    [[nodiscard]] auto& queue() const { return *_config.queue; }
    [[nodiscard]] SubmitInfo& submitInfo();
    [[nodiscard]] Fence const& fence() const;
    [[nodiscard]] size_t subIndex() const;

public:
    [[nodiscard]] bool created() const;
    [[nodiscard]] bool recording() const;

    RenderStage(RenderStage const& other) = delete;
    RenderStage& operator=(RenderStage const& other) = delete;
    RenderStage(RenderStage&& other) noexcept = default;
    RenderStage& operator=(RenderStage&& other) noexcept = default;
};

}
