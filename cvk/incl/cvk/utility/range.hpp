#pragma once
#include <volk.h>

#include <cstddef>


namespace cvk {
/**
 * @brief offset and size of a part of a buffer or a memory
 * @details in bytes, in elements for a typed buffer
 */
struct Range {
    size_t offset = 0;

    /**
     * @brief (VK_WHOLE_SIZE => to the end)
     */
    size_t size = VK_WHOLE_SIZE;
};
}
