#pragma once
#include "stage.hpp"

#include "cvk/render/sync/semaphore.hpp"

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/ptr/not_null.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>


namespace cvk {
class Image;
class ImageView;
}

namespace jvk {
class StageGraph;
}

namespace jvk::dev {
class PresentStage;

/**
 * resources of one frame slot
 * @details owned by the graph, they live as long as the graph
 * @details @ref Frame uses them for one frame
 */
struct FrameSlot {
    cth::not_null_ptr<StageGraph> graphPtr;
    size_t slot;

    /**
     * nullptr -> no present
     */
    PresentStage* presentStagePtr;

    /**
     * signaled by the acquire of the swapchain image, std::nullopt -> no present
     */
    std::optional<cvk::Semaphore> acquireSemaphoreOpt;

    /**
     * one per stage of the config
     */
    std::deque<StageSlot> stageSlots{};

    //TODO review the stages of a frame live in its slot: the vector is reused, clear and emplace per frame
    /**
     * the stages of the current frame, constructed by @ref StageGraph::nextFrame()
     */
    std::vector<Stage> stages{};
};
}

namespace jvk {
//TODO review cvk types: the swapchain owns cvk::Image values, a jvk::Image& to them is not possible
/**
 * acquired swapchain image of a frame
 * @details tracked like every image: the commands of the acquiring stage record its accesses
 */
struct SwapchainImage {
    cth::not_null_ptr<cvk::Image const> imagePtr;
    cth::not_null_ptr<cvk::ImageView const> viewPtr;

    /**
     * index of the image in the swapchain
     */
    uint32_t imageIdx;
};

/**
 * one frame
 * @details @ref StageGraph::nextFrame() constructs it per frame
 * @details the resources come from its frame slot
 * @details valid until @ref StageGraph::nextFrame() hands out its slot again
 */
class Frame {
public:
    /**
     * @param slotPtr resources of the frame slot, with the stages of the frame
     * @pre slotPtr != nullptr
     */
    Frame(dev::FrameSlot* slotPtr, uint64_t number, bool extentChanged);

    /**
     * @return the stage in this frame
     * @pre stageId < size of @ref StageGraphConfig::stages
     */
    [[nodiscard]] Stage& stage(size_t stageId);

    //TODO review the acquiring stage as a parameter
    /**
     * the swapchain image of the frame for the stage
     * @details the first call acquires it in the strand of the present queue, later calls return it
     * @details no call -> no acquire and no present
     * @details acquiring stage: its submit waits for the acquire, records the transition to
     *  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR and signals the present, the present follows
     * @return std::nullopt -> no image: skip the acquiring stage
     * @pre the graph has a present stage
     * @pre one acquiring stage per frame
     * @pre the stage did not submit or skip in this frame
     * @attention blocks the strand of the present queue while no swapchain image is free
     */
    [[nodiscard]] cth::co::executor_task<std::optional<SwapchainImage>> swapchainImage(size_t stageId);

    //TODO review public for the graph: the acquiring stage of the frame
    /**
     * @return the stage that acquired the swapchain image, std::nullopt -> no acquire
     */
    [[nodiscard]] constexpr std::optional<size_t> swapchainStageId() const { return _swapchainStageIdOpt; }

    /**
     * @return number of the frame, the first frame is 1
     */
    [[nodiscard]] constexpr uint64_t number() const { return _number; }

    [[nodiscard]] constexpr size_t slot() const { return _slotPtr->slot; }

    /**
     * @return @ref StageGraph::nextFrame() recreated the swapchain with another extent for this frame
     */
    [[nodiscard]] constexpr bool extentChanged() const { return _extentChanged; }

private:
    cth::not_null_ptr<dev::FrameSlot> _slotPtr;
    uint64_t _number;
    bool _extentChanged;

    /**
     * std::nullopt -> no acquire in this frame
     */
    std::optional<std::optional<SwapchainImage>> _swapchainImageOpt = std::nullopt;

    std::optional<size_t> _swapchainStageIdOpt = std::nullopt;

public:
    Frame(Frame const& other) = delete;
    Frame(Frame&& other) noexcept = default;
    Frame& operator=(Frame const& other) = delete;
    Frame& operator=(Frame&& other) noexcept = default;
};
}
