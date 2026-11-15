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
     * @details `driverThreads` `0` -> the workers call the driver too
     */
    size_t workers = 0;

    /**
     * threads for the vulkan driver calls, `0` -> the workers
     * @details recommended `>= 2`: a blocking acquire holds one
     */
    size_t driverThreads = 0;
};
}
