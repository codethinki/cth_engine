#pragma once
#include "CthConstants.hpp"

#ifdef CONSTANT_DEBUG_MODE
#define CTH_DEBUG_IMPL
#else
#define  CTH_DEBUG_IMPL() = default
#endif