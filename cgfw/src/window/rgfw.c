// the RGFW implementation, no other source file defines RGFW_IMPLEMENTATION
// C: the Wayland part of RGFW is no valid C++ (void* conversions, designator order)

// volk first: RGFW includes vulkan.h, volk needs VK_NO_PROTOTYPES before it
#include <volk.h>

#define RGFW_IMPLEMENTATION
#include <RGFW.h>
