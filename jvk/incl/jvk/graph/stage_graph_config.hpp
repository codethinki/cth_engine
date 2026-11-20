#pragma once
#include <volk.h>

#include <cth/io/log.hpp>

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

    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    ///< stages of the waiting submit that wait

    uint32_t frameOffset = 0;
    ///< frames before the current frame (`0` => the same frame, `1` => the previous frame)
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

namespace jvk {
constexpr void StageGraphConfig::debug_check(StageGraphConfig const& config) {
    CTH_CRITICAL(config.frameSlots == 0, "frameSlots must be > 0") {}

    auto const stageCount = config.stages.size();
    // the waits with frameOffset == 0 per stage
    std::vector<size_t> waitCounts(stageCount, 0);
    for(size_t stageId = 0; stageId < stageCount; ++stageId)
        for(auto const& wait : config.stages[stageId].waits) {
            CTH_CRITICAL(
                wait.stageId >= stageCount,
                "the waits must name stages of the config, got {} for {} stages",
                wait.stageId,
                stageCount
            ) {}
            if(wait.frameOffset == 0)
                ++waitCounts[stageId];
        }

    // removes the stages without open waits until none is left, a rest is a cycle
    std::vector<size_t> ready{};
    for(size_t stageId = 0; stageId < stageCount; ++stageId)
        if(waitCounts[stageId] == 0)
            ready.push_back(stageId);

    size_t removed = 0;
    while(!ready.empty()) {
        auto const doneId = ready.back();
        ready.pop_back();
        ++removed;

        for(size_t stageId = 0; stageId < stageCount; ++stageId)
            for(auto const& wait : config.stages[stageId].waits)
                if(wait.frameOffset == 0 && wait.stageId == doneId && --waitCounts[stageId] == 0)
                    ready.push_back(stageId);
    }

    CTH_CRITICAL(removed != stageCount, "the waits with frameOffset == 0 must form no cycle") {}
}
}
