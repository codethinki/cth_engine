#pragma once
#include "cvk/base/instance_config.hpp"

#include <cth/test.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define JVK_TEST(suite, test_name) S_TEST(jvk, suite, test_name)

namespace jvk::test {
constexpr std::string_view APP_NAME = "jvk_tests";

/**
 * instance config with `APP_NAME`
 * @param debugMessengerOpt see `cvk::InstanceConfig::debugMessenger`
 */
[[nodiscard]] inline cvk::InstanceConfig instance_config(
    std::vector<std::string> extensions = {},
    std::optional<cvk::DebugMessenger::Config> debugMessengerOpt = std::nullopt
) {
    return cvk::InstanceConfig{
        .appName = std::string{APP_NAME},
        .extensions = std::move(extensions),
        .debugMessenger = debugMessengerOpt,
    };
}

/**
 * SPIR-V of the test shaders of jvk/tests/shaders: `<name>.<stage>.spv`
 */
inline std::filesystem::path const SHADER_DIR{JVK_TEST_SHADER_DIR};
}
