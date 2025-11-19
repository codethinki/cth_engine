#pragma once
#include "jolly/utility/os/os_def.hpp"

#ifdef JLY_FS_WINDOWS
#include <boost/asio/windows/object_handle.hpp>
#elifdef JLY_FS_POSIX
#include <boost/asio/posix/stream_descriptor.hpp>
#else
#error "unsupported file system for os_asio_include.hpp"
#endif
