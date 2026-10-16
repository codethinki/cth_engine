#pragma once
#include <cth/test.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#define CVK_TEST(suite, test_name) S_TEST(cvk, suite, test_name)

namespace cvk::test {
constexpr std::string_view APP_NAME = "cvk_tests";

inline std::vector<std::string> const NO_EXTENSIONS{};

/**
 * @brief glsl sources of the test shaders
 */
inline std::filesystem::path const SHADER_DIR{CVK_TEST_SHADER_DIR};
}
