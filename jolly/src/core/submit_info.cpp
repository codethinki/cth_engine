#include "jolly/core/submit_info.hpp"


#include "jolly/render/cmd/primary_cmd_buffer.hpp"
#include "jolly/render/sync/fence.hpp"

//impl only header
#include "../render/sync/pipeline_wait_stage_helpers.hpp"



#include <jvk/base/queue/submit_info.hpp>
#include <jvk/render/sync/pipeline_wait_stage.hpp>

#include <vector>

namespace jly {

namespace {
    std::vector<jvk::PrimaryCmdBuffer const*> to_vk(SubmitInfo::cmd_buffers_span span) {
        return {std::from_range, span | std::views::transform([](auto const* buffer) { return &buffer->raw(); })};
    }

    std::vector<jvk::PipelineWaitStage> to_vk(SubmitInfo::pipeline_wait_stage_span span) {
        return {std::from_range, span | std::views::transform([](auto& stage) { return to_vk(stage); })};
    }

}

SubmitInfo::SubmitInfo(
    Core const& core,
    cmd_buffers_span cmd_buffers,
    pipeline_wait_stage_span wait_stages,
    std::span<jvk::Semaphore* const> signal_semaphores,
    bool fence_signaled
) : _cmdBuffers{std::from_range, cmd_buffers},
    _fence{core, fence_signaled},
    _submitInfo{
        std::make_unique<jvk::SubmitInfo>(
            to_vk(cmd_buffers),
            to_vk(wait_stages),
            signal_semaphores,
            &_fence.raw()
        )
    } {}
SubmitInfo& SubmitInfo::next() {
    _submitInfo->next();
    return *this;
}
}
