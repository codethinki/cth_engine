#pragma once
#include "jvk/utility/os/os_def.hpp"

#ifdef JVK_FS_WINDOWS
#include "incl_windows.hpp"
#elifdef JVK_FS_POSIX
#include "incl_posix.hpp"
#endif

