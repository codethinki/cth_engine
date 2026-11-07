#pragma once
#include "cvk/base/instance_config.hpp"

#include <cth/test.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#define CVK_TEST(suite, test_name) S_TEST(cvk, suite, test_name)

namespace cvk::test {
constexpr std::string_view APP_NAME = "cvk_tests";

/**
 * instance config with @ref APP_NAME
 * @param debugMessengerOpt see @ref InstanceConfig::debugMessenger
 */
[[nodiscard]] inline InstanceConfig instance_config(
    std::vector<std::string> extensions = {},
    std::optional<DebugMessenger::Config> debugMessengerOpt = std::nullopt
) {
    return InstanceConfig{
        .appName = std::string{APP_NAME},
        .extensions = std::move(extensions),
        .debugMessenger = debugMessengerOpt,
    };
}

/**
 * glsl sources of the test shaders
 */
inline std::filesystem::path const SHADER_DIR{CVK_TEST_SHADER_DIR};
}
