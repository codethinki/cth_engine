#pragma once
#include "cvk/base/instance_config.hpp"

#include <cth/test.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define JVK_TEST(suite, test_name) S_TEST(jvk, suite, test_name)

namespace jvk::test {
constexpr std::string_view APP_NAME = "jvk_tests";

/**
 * instance config with @ref APP_NAME
 */
[[nodiscard]] inline cvk::InstanceConfig instance_config(std::vector<std::string> extensions = {}) {
    return cvk::InstanceConfig{.appName = std::string{APP_NAME}, .extensions = std::move(extensions)};
}
}
