#include "jvk/graph/frame.hpp"

#include "jvk/graph/stage_graph.hpp"

#include "cvk/data/img/image.hpp"

#include <cth/io/log.hpp>

#include <utility>
#include <variant>


namespace jvk {
Frame::Frame(dev::FrameSlot& frameSlot, uint64_t number) : _frameSlot{frameSlot}, _number{number} {}

Stage& Frame::stage(size_t stageId) {
    auto& stages = _frameSlot->stages;
    CTH_CRITICAL(
        stageId >= stages.size(),
        "stage id must be < {}, got {}",
        stages.size(),
        stageId
    ) {}
    return stages[stageId];
}

cth::co::executor_task<std::optional<SwapchainImage>> Frame::acquire(
    Surface& surface,
    Stage& acquireStage,
    Stage& presentStage,
    VkPipelineStageFlags2 stages
) {
    CTH_CRITICAL(
        acquireStage.status() != Stage::Status::UNSUBMITTED
        || presentStage.status() != Stage::Status::UNSUBMITTED,
        "the acquire stage and the present stage must be unsubmitted"
    ) {}
    CTH_CRITICAL(
        &acquireStage.graph() != &*_frameSlot->graph
        || &presentStage.graph() != &*_frameSlot->graph
        || acquireStage.frameNumber() != _number || presentStage.frameNumber() != _number,
        "the acquire stage and the present stage must belong to this frame"
    ) {}

    auto imageOpt = co_await surface.acquire();
    if(!imageOpt.has_value())
        co_return std::nullopt;

    auto& image = *imageOpt;
    SwapchainImage const swapchainImage{
        .image = image.image(),
        .view = image.view(),
        .imageIdx = image.imageIdx(),
        .extent = image.extent(),
    };

    // the first barrier of the image chains with the wait
    if(auto const* const semaphorePtr =
        std::get_if<cth::ref_wrapper<cvk::Semaphore const>>(&image.wait()))
        acquireStage.await(**semaphorePtr, stages);
    else {
        auto ready = std::get<SemaphoreSubmitInfo>(image.wait());
        ready.stages = stages;
        acquireStage.await(ready);
    }
    _frameSlot->graph->assume(
        *swapchainImage.image,
        ImageState{.layout = VK_IMAGE_LAYOUT_UNDEFINED, .scope = {.stages = stages}}
    );

    presentStage.present(std::move(image));
    co_return swapchainImage;
}
}
