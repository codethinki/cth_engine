#pragma once

#include "jvk/render/cmd/cmd_buffer_config.hpp"

namespace jly {
struct SecondaryCmdBufferConfig {
    jvk::CmdBufferConfig baseConfig;
    bool taskReuse = false;
};
}
