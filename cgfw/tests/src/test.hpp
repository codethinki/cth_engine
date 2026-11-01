#pragma once
#include "cgfw/window/window.hpp"

#include "cvk/base/instance_config.hpp"

#include <cth/test.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#define CGFW_TEST(suite, test_name) S_TEST(cgfw, suite, test_name)

/**
 * skips the test without a display
 */
#define CGFW_SKIP_WITHOUT_DISPLAY() \
    if(!cgfw::test::display_available()) GTEST_SKIP() << "no display"

namespace cgfw::test {
constexpr std::string_view APP_NAME = "cgfw_tests";

/**
 * @return a display exists, linux: WAYLAND_DISPLAY is set
 */
[[nodiscard]] bool display_available();

/**
 * hidden window: a test run shows no window
 */
[[nodiscard]] inline Window::Config window_config(std::string title = "cgfw test") {
    return Window::Config{.title = std::move(title), .extent = {64, 64}, .flags = WindowFlags::HIDE};
}

/**
 * instance config with @ref APP_NAME
 * @param debugMessenger see @ref cvk::InstanceConfig::debugMessenger
 */
[[nodiscard]] inline cvk::InstanceConfig instance_config(
    std::vector<std::string> extensions = {},
    std::optional<cvk::DebugMessenger::Config> debugMessenger = std::nullopt
) {
    return cvk::InstanceConfig{
        .appName = std::string{APP_NAME},
        .extensions = std::move(extensions),
        .debugMessenger = debugMessenger,
    };
}
}
