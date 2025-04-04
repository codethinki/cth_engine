#pragma once
#include "RenderStageConfig.hpp"
#include "src/vulkan/render/control/CthPipelineWaitStage.hpp"
#include "src/vulkan/utility/cth_constants.hpp"



namespace cth::vk {
class Fence;
}

namespace cth::vk {
class SecondaryCmdBuffer;
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

struct RenderStageCmdBuffers {
    PrimaryCmdBuffer* cmdBuffer;
    std::vector<SecondaryCmdBuffer*> secondaryCmdBuffers;
};

class RenderStage {
public:
    constexpr static uint32_t GROUP_SIZE = constants::FRAMES_IN_FLIGHT;

    using Config = RenderStageConfig;
    //TEMP left off here implement this
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
     * @return cmd buffers
     */
    RenderStageCmdBuffers begin();

    /**
     * @brief calls @ref PrimaryCmdBuffer::end()
     * @attention requires @ref recording()
     */
    void end();
    void optEnd() { if(recording()) end(); }

    /**
     * @brief submits the stage to the queue
     * @details calls:
        - @ref optEnd()
        - @ref Queue::submit(SubmitInfo const&);
     */
    void submit();



    /**
     * @brief skip submits the stage
     * @note triggers sync primitives
     */
    void skip();

    /**
     * @brief waits or times out
     * @param timeout in nanoseconds
     * @return @ref Fence::wait(size_t)
     */
    [[nodiscard]] VkResult wait(size_t timeout) const;

    /**
     * @brief calls @ref wait(size_t) with UINT64_MAX
     */
    void wait() const;

private:
    void initCmdPools();
    void initCmdBuffers();
    void initFences();
    void initSubmitInfos();

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

    Config _config;

    VkExtent2D _extent{};
    std::vector<CmdPool> _cmdPools;
    std::vector<PrimaryCmdBuffer> _primaryCmdBuffers;
    std::vector<SecondaryCmdBuffer> _secondaryCmdBuffers;
    std::vector<Fence> _fences;
    std::vector<SubmitInfo> _submitInfos;


    size_t _subIndex = 0;

    [[nodiscard]] size_t subIndex() const { return _subIndex; }
    [[nodiscard]] size_t secondaryChunkSize() const;
    [[nodiscard]] PrimaryCmdBuffer& primaryCmdBuffer();
    [[nodiscard]] std::vector<SecondaryCmdBuffer*> secondaryCmdBuffers();



    [[nodiscard]] auto& queue() const { return *_config.queue; }
    [[nodiscard]] SubmitInfo& submitInfo();
    [[nodiscard]] Fence const& fence() const;

public:
    [[nodiscard]] bool created() const;
    [[nodiscard]] bool recording() const;

    RenderStage(RenderStage const& other) = delete;
    RenderStage& operator=(RenderStage const& other) = delete;
    RenderStage(RenderStage&& other) noexcept = default;
    RenderStage& operator=(RenderStage&& other) noexcept = default;
};

}
