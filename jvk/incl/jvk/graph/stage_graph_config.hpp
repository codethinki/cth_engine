#pragma once
#include <volk.h>

#include <cth/ptr/not_null.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>


namespace cvk {
class Swapchain;
}

namespace jvk {
/**
 * wait of a submit for a stage
 * @details frameOffset > 0: timeline wait for an earlier frame of the stage
 */
struct StageWait {
    /**
     * index in @ref StageGraphConfig::stages
     */
    size_t stage;

    /**
     * stages of the waiting submit that wait
     */
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    /**
     * frames before the current frame (0 => the same frame, 1 => the previous frame)
     */
    uint32_t frameOffset = 0;
};

/**
 * config of a @ref Stage
 */
struct StageConfig {
    /**
     * role of @ref Core::queue()
     */
    size_t queue;

    std::vector<StageWait> waits{};
};

/**
 * config of the present stage
 */
struct PresentStageConfig {
    /**
     * role of @ref Core::queue() that presents
     * @pre the queue can present to the surface of the swapchain
     */
    size_t queue;

    /**
     * @pre created, outlives the graph
     */
    cth::not_null_ptr<cvk::Swapchain> swapchain;

    /**
     * index in @ref StageGraphConfig::stages of the stage that writes the swapchain image
     */
    size_t imageStage;
};

/**
 * config of a @ref StageGraph
 */
struct StageGraphConfig {
    //TODO review id = index instead of the hash map of the old sketch
    /**
     * the stages, the id of a stage is its index
     */
    std::vector<StageConfig> stages{};

    /**
     * std::nullopt -> no present
     */
    std::optional<PresentStageConfig> present = std::nullopt;

    /**
     * frames that the gpu works on at the same time
     * @pre > 0
     */
    size_t frameSlots = 2;

    /**
     * checks the config
     * @details checks:
     * - the waits and the image stage name stages of the config
     * - the waits with frameOffset 0 form no cycle
     */
    static constexpr void debug_check(StageGraphConfig const&);
};
}
