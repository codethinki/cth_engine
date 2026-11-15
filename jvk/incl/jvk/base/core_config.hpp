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
     * @details `queueWorkers` `0` -> the strands of the queues run on the workers too
     */
    size_t workers = 0;

    /**
     * threads of an own scheduler for the strands of the queues, `0` -> the strands run on the workers
     * @details `0`: a free strand runs a submit, an acquire or a present inline on the calling worker, a warm thread
     * @details `> 0`: the submits and the blocking calls (FIFO acquire, `Queue::waitIdle()`) run apart from the
     *  worker load, every entry from a worker posts
     */
    size_t queueWorkers = 0;
};
}
