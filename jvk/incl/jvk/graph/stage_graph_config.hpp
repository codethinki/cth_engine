#pragma once
#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <vector>


namespace jvk {
/**
 * wait of a submit for a stage
 * @details frameOffset > 0: timeline wait for an earlier frame of the stage
 */
struct StageWait {
    /**
     * id of the waited stage, see @ref StageGraphConfig::stages
     */
    size_t stageId;

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
     * index of the queue in @ref cvk::CoreConfig::queueProperties
     */
    size_t queueIdx;

    std::vector<StageWait> waits{};

    /**
     * secondary command buffers per frame slot, see @ref Stage::secondary()
     */
    size_t secondaries = 0;
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
     * frames that the gpu works on at the same time
     * @pre > 0
     */
    size_t frameSlots = 2;

    /**
     * checks the config
     * @details checks:
     * - the waits name stages of the config
     * - the waits with frameOffset 0 form no cycle
     */
    static constexpr void debug_check(StageGraphConfig const&);
};
}
