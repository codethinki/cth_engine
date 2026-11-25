#include "context.hpp"
#include "test.hpp"

#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/os/osdef.hpp>

#ifdef CTH_PLATFORM_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif

#include <filesystem>
#include <format>
#include <fstream>
#include <system_error>


namespace cvk {
namespace {
    std::filesystem::path const TRIANGLE_VERT = test::SHADER_DIR / "triangle.vert.spv";

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
     * writes bytes to a file in `temp_dir()`
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
    auto const spv = Shader::load(TRIANGLE_VERT);

    Shader const shader{context.core, VK_SHADER_STAGE_VERTEX_BIT, spv};

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

    Shader const shader{context.core, VK_SHADER_STAGE_VERTEX_BIT, TRIANGLE_VERT};

    EXPECT_TRUE(shader.created());
}

CVK_TEST(Shader, load_reads_the_words) {
    constexpr uint32_t SPIRV_MAGIC = 0x07230203;

    auto const spv = Shader::load(TRIANGLE_VERT);

    EXPECT_EQ(spv.size() * sizeof(uint32_t), std::filesystem::file_size(TRIANGLE_VERT));
    ASSERT_FALSE(spv.empty());
    EXPECT_EQ(spv.front(), SPIRV_MAGIC);
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
    Shader other{
        context.core,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        test::SHADER_DIR / "triangle.frag.spv"
    };
    auto const vkShader = other.get();

    shader = std::move(other);

    EXPECT_EQ(shader.get(), vkShader);
    EXPECT_EQ(shader.stage(), VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_FALSE(other.created());
}

CVK_TEST(Shader, decompose_returns_the_released_handle) {
    // a leak shows up when the shared device is destroyed
    test::Context const context{};
    Shader shader{context.core, VK_SHADER_STAGE_VERTEX_BIT, TRIANGLE_VERT};
    auto const vkShader = shader.get();

    auto const [vkReleased] = shader.release().decompose();
    EXPECT_EQ(vkReleased, vkShader);

    cvk::destroy(context.core.deviceTable(), vkReleased);
}
}
