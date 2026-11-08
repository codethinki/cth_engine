#pragma once
#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <vector>


namespace jvk {
/**
 * wait of a submit for a stage
 * @details `frameOffset > 0` -> timeline wait for an earlier frame of the stage
 */
struct StageWait {
    size_t stageId; ///< id of the waited stage, see `StageGraphConfig::stages`

    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; ///< stages of the waiting submit that wait

    uint32_t frameOffset = 0; ///< frames before the current frame (`0` => the same frame, `1` => the previous frame)
};

/**
 * config of a `Stage`
 */
struct StageConfig {
    size_t queueIdx; ///< index of the queue in `cvk::CoreConfig::queueProperties`

    std::vector<StageWait> waits{};

    /**
     * secondary command buffers per frame slot
     * @see `Stage::secondary()`
     */
    size_t secondaries = 0;
};

/**
 * config of a `StageGraph`
 */
struct StageGraphConfig {
    //TODO review id = index instead of the hash map of the old sketch
    std::vector<StageConfig> stages{}; ///< the stages, the id of a stage is its index

    /**
     * frames that the gpu works on at the same time
     * @pre `frameSlots > 0`
     */
    size_t frameSlots = 2;

    /**
     * checks the config
     * @details checks:
     * - the waits name stages of the config
     * - the waits with `frameOffset == 0` form no cycle
     */
    static constexpr void debug_check(StageGraphConfig const&);
};
}
