#pragma once
#include "RenderStageConfig.hpp"

#include "jolly/utility/types.hpp"


//IMPLEMENT release and state
namespace jvk {
class CmdPool;

class Queue;
class RenderPass;
class Semaphore;
}

namespace jly {
class Core;
class Fence;
class SubmitInfo;
class PrimaryCmdBuffer;
class RenderPulse;
class SecondaryCmdBuffer;
}

namespace jly {


struct RenderStageCmdBuffers {
    /**
     * render stage
     */
    PrimaryCmdBuffer* cmdBuffer;
    /**
     * represent sub stages
     */
    std::vector<SecondaryCmdBuffer*> secondaryCmdBuffers;
};

class RenderStage {
public:
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
     * @brief waits for stage completion
     * @details calls @ref Fence::wait() const
     */
    void wait() const;

private:
    void reset();

    void initCmdPools();
    void initCmdBuffers();
    void initSubmitInfos();

    void init();

    /**
     * @details:
        - parallel frames in flight -> pool per frame
        - parallel sub stages -> pool per sub stage
     */
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

    not_null<Core const*> _core;
    not_null<RenderPulse const*> _pulse;

    Config _config;

    std::vector<jvk::CmdPool> _cmdPools;

    std::vector<PrimaryCmdBuffer> _primaryCmdBuffers; //count: GROUP_SIZE
    std::vector<SecondaryCmdBuffer> _secondaryCmdBuffers; //count: subStages * GROUP_SIZE
    std::vector<SubmitInfo> _submitInfos; //count: GROUP_SIZE



    [[nodiscard]] size_t secondaryChunkSize() const;
    [[nodiscard]] PrimaryCmdBuffer& primaryCmdBuffer();
    [[nodiscard]] PrimaryCmdBuffer const& primaryCmdBuffer() const;
    [[nodiscard]] std::vector<SecondaryCmdBuffer*> secondaryCmdBuffers();



    [[nodiscard]] auto& queue() const { return *_config.queue; }
    [[nodiscard]] SubmitInfo& submitInfo();
    [[nodiscard]] SubmitInfo const& submitInfo() const;

    [[nodiscard]] size_t subIndex() const;
    [[nodiscard]] size_t framesInFlight() const;

public:
    [[nodiscard]] bool created() const;
    [[nodiscard]] bool recording() const;

    RenderStage(RenderStage const& other) = delete;
    RenderStage& operator=(RenderStage const& other) = delete;
    RenderStage(RenderStage&& other) noexcept;
    RenderStage& operator=(RenderStage&& other) noexcept = default;
};

}
