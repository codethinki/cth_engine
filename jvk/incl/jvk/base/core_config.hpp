#pragma once
#include "cvk/base/core_config.hpp"

#include <cstddef>


namespace jvk {
/**
 * config of a `Core`
 */
struct CoreConfig {
    /**
     * queues, features, extensions and device of the cvk core
     * @details empty `cvk::CoreConfig::queueSets` -> `queue_sets()` of `cvk::CoreConfig::queueProperties`
     */
    cvk::CoreConfig vk{};

    /**
     * threads of the worker scheduler, `0` -> `std::thread::hardware_concurrency()`
     * @details `std::thread::hardware_concurrency()` is `0` -> one thread
     * @details the strands of the queues run on the workers
     */
    size_t workers = 0;
};
}
