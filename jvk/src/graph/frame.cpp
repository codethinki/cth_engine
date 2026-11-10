#include "jvk/graph/frame.hpp"

#include <cth/io/log.hpp>


namespace jvk {
Frame::Frame(dev::FrameSlot& frameSlot, uint64_t number) : _frameSlot{frameSlot}, _number{number} {}

Stage& Frame::stage(size_t stageId) {
    auto& stages = _frameSlot->stages;
    CTH_CRITICAL(stageId >= stages.size(), "stage id must be < {}, got {}", stages.size(), stageId) {}
    return stages[stageId];
}
}
