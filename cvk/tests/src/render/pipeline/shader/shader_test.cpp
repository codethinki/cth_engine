#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/os/osdef.hpp>

#ifdef CTH_PLATFORM_WINDOWS
#include <process.h>
#define CVK_NULL_DEVICE "NUL"
#else
#include <unistd.h>
#define CVK_NULL_DEVICE "/dev/null"
#endif

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <system_error>


namespace cvk {
namespace {
    using test::shaders::TRIANGLE_VERT;

    /**
     * @return glslc runs, the check runs once per process
     */
    bool glslc_available() {
        static bool const available = std::system("glslc --version >" CVK_NULL_DEVICE " 2>&1") == 0;
        return available;
    }

    auto process_id() {
#ifdef CTH_PLATFORM_WINDOWS
        return _getpid();
#else
        return getpid();
#endif
    }

    /**
     * @return directory of this process for the temp files, the process exit removes it
     * @details one directory per process: test processes can run at the same time
     */
    std::filesystem::path const& temp_dir() {
        struct Directory {
            Directory() { std::filesystem::create_directories(path); }
            ~Directory() {
                std::error_code error{};
                std::filesystem::remove_all(path, error);
            }

            std::filesystem::path path = std::filesystem::temp_directory_path()
                / std::format("cvk_tests_{}", process_id());
        };
        static Directory const directory{};
        return directory.path;
    }

    /**
     * @brief writes bytes to a file in @ref temp_dir()
     */
    std::filesystem::path write_temp(std::string_view name, std::span<char const> bytes) {
        auto path = temp_dir() / name;
        std::ofstream file{path, std::ios::binary};
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return path;
    }
}

CVK_TEST(Shader, base_constructor_does_not_create) {
    test::Context const context{};

    Shader const shader{context.core, VK_SHADER_STAGE_VERTEX_BIT};

    EXPECT_FALSE(shader.created());
    EXPECT_EQ(shader.stage(), VK_SHADER_STAGE_VERTEX_BIT);
}

CVK_TEST(Shader, create_from_spv) {
    test::Context const context{};

    Shader const shader{context.core, VK_SHADER_STAGE_VERTEX_BIT, TRIANGLE_VERT};

    ASSERT_TRUE(shader.created());

    auto const info = shader.stageInfo();
    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    EXPECT_EQ(info.stage, VK_SHADER_STAGE_VERTEX_BIT);
    EXPECT_EQ(info.module, shader.get());
    EXPECT_STREQ(info.pName, "main");
    EXPECT_EQ(info.pSpecializationInfo, nullptr);
}

CVK_TEST(Shader, create_from_file) {
    test::Context const context{};
    std::span const bytes{reinterpret_cast<char const*>(TRIANGLE_VERT.data()), sizeof(TRIANGLE_VERT)};
    auto const path = write_temp("cvk_test_triangle.vert.spv", bytes);

    Shader const shader{context.core, VK_SHADER_STAGE_VERTEX_BIT, path};

    EXPECT_TRUE(shader.created());
    EXPECT_TRUE(std::ranges::equal(Shader::load(path), TRIANGLE_VERT));

    std::filesystem::remove(path);
}

CVK_TEST(Shader, load_rejects_bad_files) {
    auto const missing = temp_dir() / "cvk_test_missing.spv";
    EXPECT_THROW([[maybe_unused]] auto const spv = Shader::load(missing), cvk_exception);

    constexpr std::array<char, 5> notWords{1, 2, 3, 4, 5};
    auto const odd = write_temp("cvk_test_odd.spv", notWords);
    EXPECT_THROW([[maybe_unused]] auto const spv = Shader::load(odd), cvk_exception);
    std::filesystem::remove(odd);

    auto const empty = write_temp("cvk_test_empty.spv", {});
    EXPECT_THROW([[maybe_unused]] auto const spv = Shader::load(empty), cvk_exception);
    std::filesystem::remove(empty);
}

CVK_TEST(Shader, release_and_wrap) {
    test::Context const context{};
    Shader shader{context.core, VK_SHADER_STAGE_VERTEX_BIT, TRIANGLE_VERT};
    auto const vkShader = shader.get();

    auto const state = shader.release();
    EXPECT_FALSE(shader.created());

    Shader const wrapped{context.core, VK_SHADER_STAGE_VERTEX_BIT, state};
    EXPECT_EQ(wrapped.get(), vkShader);
}

CVK_TEST(Shader, move_assignment_destroys_the_old_shader) {
    test::Context const context{};
    Shader shader{context.core, VK_SHADER_STAGE_VERTEX_BIT, TRIANGLE_VERT};
    Shader other{context.core, VK_SHADER_STAGE_FRAGMENT_BIT, test::shaders::TRIANGLE_FRAG};
    auto const vkShader = other.get();

    shader = std::move(other);

    EXPECT_EQ(shader.get(), vkShader);
    EXPECT_EQ(shader.stage(), VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_FALSE(other.created());
}

CVK_TEST(Shader, compile_and_create) {
    if(!glslc_available()) GTEST_SKIP() << "glslc not found";
    test::Context const context{};
    auto const spvPath = temp_dir() / "cvk_test_compiled.vert.spv";

    Shader const shader{context.core, VK_SHADER_STAGE_VERTEX_BIT, spvPath, test::SHADER_DIR / "triangle.vert"};

    EXPECT_TRUE(shader.created());
    EXPECT_TRUE(std::filesystem::exists(spvPath));
    EXPECT_FALSE(std::filesystem::exists(spvPath.string() + ".log"));

    std::filesystem::remove(spvPath);
}

CVK_TEST(Shader, compile_error_throws) {
    if(!glslc_available()) GTEST_SKIP() << "glslc not found";
    constexpr std::string_view broken = "#version 450\nvoid main() { undeclared = 1; }\n";
    auto const glslPath = write_temp("cvk_test_broken.vert", broken);
    auto const spvPath = temp_dir() / "cvk_test_broken.vert.spv";

    EXPECT_THROW(Shader::compile(glslPath, spvPath), cvk_exception);
    EXPECT_FALSE(std::filesystem::exists(spvPath.string() + ".log"));

    std::filesystem::remove(glslPath);
}

CVK_TEST(Shader, compile_missing_glsl_throws) {
    auto const missing = temp_dir() / "cvk_test_missing.vert";

    EXPECT_THROW(Shader::compile(missing, missing.string() + ".spv"), cvk_exception);
}
}
