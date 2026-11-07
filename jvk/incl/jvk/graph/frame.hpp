#pragma once
#include "stage.hpp"

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/ptr/not_null.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>


namespace cvk {
class Image;
class ImageView;
}

namespace jvk {
class StageGraph;

//TODO review cvk types: the swapchain owns cvk::Image values, a jvk::Image& to them is not possible
/**
 * acquired swapchain image of a frame
 * @details tracked like every image: the commands of the image stage record its accesses
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
 * frame of one frame slot
 * @details the graph creates one per frame slot, @ref StageGraph::nextFrame() hands it out
 * @details valid until @ref StageGraph::nextFrame() hands out its slot again
 */
class Frame {
public:
    /**
     * constructs the stages of the frame slot
     * @details the graph constructs one per frame slot
     * @throws cvk::vk_result_exception result of a create of a stage
     */
    Frame(StageGraph&, size_t slot);

    /**
     * @return the stage in this frame
     * @pre stageId < size of @ref StageGraphConfig::stages
     */
    [[nodiscard]] Stage& stage(size_t stageId);

    /**
     * the swapchain image of the frame
     * @details the first call acquires it in the strand of the present queue, later calls return it
     * @details no call -> no acquire and no present
     * @details the present follows the submit of the image stage
     * @return std::nullopt -> no image: skip the image stage
     * @pre the graph has a present stage
     * @attention blocks the strand of the present queue while no swapchain image is free
     */
    [[nodiscard]] cth::co::executor_task<std::optional<SwapchainImage>> swapchainImage();

    /**
     * @return number of the frame, the first frame is 1
     */
    [[nodiscard]] constexpr uint64_t number() const { return _number; }

    [[nodiscard]] constexpr size_t slot() const { return _slot; }

    /**
     * @return @ref StageGraph::nextFrame() recreated the swapchain with another extent for this frame
     */
    [[nodiscard]] constexpr bool extentChanged() const { return _extentChanged; }

    /**
     * prepares the slot and its stages for a new frame
     * @details called by @ref StageGraph::nextFrame()
     * @pre the fences of the last submits of the slot signaled
     */
    void beginFrame(uint64_t frameNumber, bool extentChanged);

private:
    cth::not_null_ptr<StageGraph> _graphPtr;
    size_t _slot;
    uint64_t _number = 0;
    bool _extentChanged = false;

    std::vector<std::unique_ptr<Stage>> _stages{};

    /**
     * std::nullopt -> no acquire in this frame
     */
    std::optional<std::optional<SwapchainImage>> _swapchainImageOpt = std::nullopt;

public:
    Frame(Frame const& other) = delete;
    Frame(Frame&& other) = delete;
    Frame& operator=(Frame const& other) = delete;
    Frame& operator=(Frame&& other) = delete;
};
}
