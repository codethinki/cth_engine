#pragma once
#include "cvk/base/core_config.hpp"
#include "cvk/data/descriptor/resource_heap.hpp"
#include "cvk/data/descriptor/sampler_heap.hpp"
#include "cvk/utility/device/device_features.hpp"

#include <cstddef>


namespace jvk::constants {
/**
 * features that the jvk core enables besides `cvk::constants::BASE_DEVICE_FEATURES`
 * @details `imageCubeArray`: `jvk/heaps.glsl` declares `jvk_texturesCubeArray`, every shader with it has the capability
 * @details `shaderStorageImageReadWithoutFormat`, `shaderStorageImageWriteWithoutFormat`: the `jvk_images*` arrays
 *  of `jvk/heaps.glsl` have no format
 */
inline constexpr cvk::DeviceFeatures DEVICE_FEATURES{
    .imageCubeArray = true,
    .shaderStorageImageReadWithoutFormat = true,
    .shaderStorageImageWriteWithoutFormat = true,
};
}

namespace jvk {
/**
 * descriptors of the heaps of a `Core`
 * @details a heap whose counts are all `0` -> no heap
 */
struct HeapsConfig {
    cvk::ResourceHeap::Config resources{
        .sampledImages = 65536,
        .storageImages = 4096,
        .inputAttachments = 64
    };
    cvk::SamplerHeap::Config samplers{.samplers = 256};

    /**
     * @return a count of `resources` is not `0`
     */
    [[nodiscard]] constexpr bool createsResourceHeap() const {
        return resources.sampledImages != 0 || resources.storageImages != 0
            || resources.inputAttachments != 0;
    }

    /**
     * @return a count of `samplers` is not `0`
     */
    [[nodiscard]] constexpr bool createsSamplerHeap() const { return samplers.samplers != 0; }
};

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
    size_t workers = 4;

    /**
     * threads for the vulkan driver calls, `0` -> the workers
     * @details recommended `>= 2`: a blocking acquire holds one
     */
    size_t driverThreads = 0;

    HeapsConfig heaps{}; ///< see `Core::resourceHeap()`, `Core::samplerHeap()`
};
}
