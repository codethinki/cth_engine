#pragma once
#include "cth_constants.hpp"

#ifdef CONSTANT_DEBUG_MODE
#define CTH_DEBUG_IMPL
#else
#define  CTH_DEBUG_IMPL = default
#endif