#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <filesystem>
#include <fstream>


namespace cvk {
namespace {
    using test::shaders::TRIANGLE_VERT;

    /**
     * @brief writes bytes to a file in the temp directory
     */
    std::filesystem::path write_temp(std::string_view name, std::span<char const> bytes) {
        auto path = std::filesystem::temp_directory_path() / name;
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
    auto const missing = std::filesystem::temp_directory_path() / "cvk_test_missing.spv";
    std::filesystem::remove(missing);
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
}
