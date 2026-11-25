#pragma once
// the state of RGFW for window.cpp, rgfw.c defines the functions: only there RGFW_info is complete

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @return windows of RGFW, `0` -> no window or RGFW is not initialized
 */
size_t cgfw_rgfw_window_count(void);

#ifdef __cplusplus
}
#endif
