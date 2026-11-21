// the RGFW implementation, no other source file defines RGFW_IMPLEMENTATION
// C: the Wayland part of RGFW is no valid C++ (void* conversions, designator order)
#include "window/rgfw_info.h"

// volk first: RGFW includes vulkan.h, volk needs VK_NO_PROTOTYPES before it
#include <volk.h>

#define RGFW_IMPLEMENTATION
#include <RGFW.h>

size_t cgfw_rgfw_window_count(void) {
    RGFW_info const* const info = RGFW_getInfo();
    return info == NULL ? 0 : (size_t)info->windowCount;
}
