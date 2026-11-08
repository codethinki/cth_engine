#pragma once
#include <volk.h>

#include <cstddef>


namespace cvk {
/**
 * range of a buffer or a memory
 * @details in bytes, in elements for a typed buffer
 */
struct Range {
    size_t offset = 0;

    size_t size = VK_WHOLE_SIZE; ///< (`VK_WHOLE_SIZE` => to the end)
};
}
