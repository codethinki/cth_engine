#pragma once
#include "cvk/debug/debug_messenger.hpp"

#include <cstddef>


namespace cvk::test {
/**
 * @brief messenger config that logs every message and counts the errors
 * @details for the instance create info too -> counts the errors of vkDestroyInstance(), e.g. leaked objects
 */
[[nodiscard]] DebugMessenger::Config counting_messenger();

/**
 * @return errors since the last @ref reset_validation_errors()
 */
[[nodiscard]] size_t validation_errors();

void reset_validation_errors();
}
