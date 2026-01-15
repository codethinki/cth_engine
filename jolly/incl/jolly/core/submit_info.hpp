#pragma once
#include "jolly/render/sync/fence.hpp"
#include "jolly/render/sync/pipeline_wait_stage.hpp"

#include <span>

namespace jvk {
struct SubmitInfo;
class Semaphore;
}

namespace jly {
class PrimaryCmdBuffer;
class Fence;

}

namespace jly {

class SubmitInfo {
public:
    using cmd_buffers_span = std::span<PrimaryCmdBuffer const* const>;
    using pipeline_wait_stage_span = std::span<PipelineWaitStage const>;

    SubmitInfo(
        Core const&,
        cmd_buffers_span,
        pipeline_wait_stage_span,
        std::span<jvk::Semaphore* const> signal_semaphores,
        bool fence_signaled
    );

    auto const& cmdBuffers() const { return _cmdBuffers; }

    auto const& fence() const { return _fence; }

    void reset_fence() { _fence.reset(); }
    void wait_reset() { _fence.waitReset(); }

    void wait() const { _fence.wait(); }

    SubmitInfo& next();

private:
    std::vector<PrimaryCmdBuffer const*> _cmdBuffers;

    Fence _fence;

    std::unique_ptr<jvk::SubmitInfo> _submitInfo;

public:
    auto const& raw() const { return *_submitInfo; }
};
}
