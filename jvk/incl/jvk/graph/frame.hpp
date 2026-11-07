#pragma once
#include "stage.hpp"

#include <cth/ptr/not_null.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>


namespace jvk {
class StageGraph;
}

namespace jvk::dev {
struct FrameSlot;
}

namespace jvk {
/**
 * one frame
 * @details
 * - per frame: constructed by `StageGraph::nextFrame()`
 * - the resources come from its frame slot
 * - valid until `StageGraph::nextFrame()` hands out its slot again
 */
class Frame {
public:
    /**
     * @param slotPtr resources of the frame slot, with the stages of the frame
     * @pre `slotPtr != nullptr`
     */
    Frame(dev::FrameSlot* slotPtr, uint64_t number);

    /**
     * @return the stage in this frame
     * @pre `stageId < StageGraphConfig::stages.size()`
     */
    [[nodiscard]] Stage& stage(size_t stageId);

    /**
     * @return number of the frame, the first frame is `1`
     */
    [[nodiscard]] constexpr uint64_t number() const { return _number; }

    [[nodiscard]] constexpr size_t slot() const;

private:
    cth::not_null_ptr<dev::FrameSlot> _slotPtr;
    uint64_t _number;

public:
    Frame(Frame const& other) = delete;
    Frame(Frame&& other) noexcept = default;
    Frame& operator=(Frame const& other) = delete;
    Frame& operator=(Frame&& other) noexcept = default;
};
}

namespace jvk::dev {
/**
 * resources of one frame slot
 * @details
 * - owned by the graph, live as long as the graph
 * - the `Frame` of each frame uses them
 */
struct FrameSlot {
    cth::not_null_ptr<StageGraph> graphPtr;
    size_t slot;

    /**
     * one per stage of the config
     */
    std::deque<StageSlot> stageSlots{};

    /**
     * the stages of the current frame, constructed by `StageGraph::nextFrame()`
     */
    std::vector<Stage> stages{};

    /**
     * the current frame of the slot, constructed by `StageGraph::nextFrame()`
     * @details `std::nullopt` -> the slot had no frame yet
     */
    std::optional<Frame> frameOpt = std::nullopt;
};
}

namespace jvk {
constexpr size_t Frame::slot() const { return _slotPtr->slot; }
}
