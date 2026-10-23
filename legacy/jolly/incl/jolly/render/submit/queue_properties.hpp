#pragma once
#include <cth/enums.hpp>

namespace jly {
using cth::en::flag_val;

enum class QueueProperties {
    NONE = 0,
    GRAPHICS = flag_val(0),
    COMPUTE = flag_val(1),
    TRANSFER = flag_val(2),
    PRESENT = flag_val(3)
};


}

CTH_GEN_ENUM_FLAG_OVERLOADS(jly::QueueProperties)

