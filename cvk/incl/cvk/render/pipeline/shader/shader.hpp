#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>


namespace cvk {
class Core;

/**
 * @brief VkShaderModule wrapper
 * @details a pipeline does not use the module after its creation
 */
class Shader {
public:
    using vk_handle_t = VkShaderModule;
    struct State;
    using path_t = std::filesystem::path;

    static constexpr std::string_view DEFAULT_COMPILER = "glslc";

    /**
     * @brief base constructor
     * @param stage of the shader
     * @pre @ref Core::created()
     */
    Shader(Core const&, VkShaderStageFlagBits stage);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Shader(Core const&, VkShaderStageFlagBits)
     * - @ref wrap()
     */
    Shader(Core const&, VkShaderStageFlagBits stage, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Shader(Core const&, VkShaderStageFlagBits)
     * - @ref create(std::span<uint32_t const>)
     */
    Shader(Core const&, VkShaderStageFlagBits stage, std::span<uint32_t const> spv);

    /**
     * @brief constructs, loads and creates
     * @details calls:
     * - @ref Shader(Core const&, VkShaderStageFlagBits)
     * - @ref create(path_t const&)
     */
    Shader(Core const&, VkShaderStageFlagBits stage, path_t const& spvPath);

    /**
     * @brief constructs, compiles the glsl file, loads and creates
     * @details for development, release builds compile shaders at build time
     *  calls:
     * - @ref compile()
     * - @ref Shader(Core const&, VkShaderStageFlagBits, path_t const&)
     * @note warns in release builds
     */
    Shader(
        Core const&,
        VkShaderStageFlagBits stage,
        path_t const& spvPath,
        path_t const& glslPath,
        path_t const& compilerPath = DEFAULT_COMPILER
    );

    /**
     * @note calls @ref optDestroy()
     */
    ~Shader() { optDestroy(); }

    /**
     * @brief wraps the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the module
     * @param spv SPIR-V code
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateShaderModule()
     */
    void create(std::span<uint32_t const> spv);

    /**
     * @brief loads the SPIR-V file and creates the module
     * @details calls:
     * - @ref load()
     * - @ref create(std::span<uint32_t const>)
     */
    void create(path_t const& spvPath);

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @return stage info for a pipeline
     * @param specialization nullptr -> no specialization constants
     * @param entryPoint name of the entry function
     * @pre @ref created()
     * @attention @ref specialization and @ref entryPoint must outlive the pipeline creation
     */
    [[nodiscard]] VkPipelineShaderStageCreateInfo stageInfo(
        VkSpecializationInfo const* specialization = nullptr,
        char const* entryPoint = "main"
    ) const;

    /**
     * @brief reads a SPIR-V file
     * @throws cvk::cvk_exception reason: file missing, size not a multiple of 4 or 0
     */
    [[nodiscard]] static std::vector<uint32_t> load(path_t const& spvPath);

    /**
     * @brief compiles a glsl file to a SPIR-V file
     * @param compilerPath glslc compatible compiler
     * @param flags for the compiler
     * @details compiler output without error -> logged as warning
     * @throws cvk::cvk_exception reason: glsl file missing, compiler failed, details: compiler output
     */
    static void compile(
        path_t const& glslPath,
        path_t const& spvPath,
        path_t const& compilerPath = DEFAULT_COMPILER,
        std::string_view flags = "-O"
    );

    static void destroy(DeviceTable table, VkShaderModule);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr VkShaderStageFlagBits stage() const { return _stage; }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }

private:
    void forget();

    core_handle<Shader> _handle;
    VkShaderStageFlagBits _stage;

public:
    Shader(Shader const& other) = delete;
    Shader(Shader&& other) noexcept = default;
    Shader& operator=(Shader const& other) = delete;
    Shader& operator=(Shader&& other) noexcept = default;

    static constexpr void debug_check(Shader const&);
};
}

namespace cvk {
struct Shader::State {
    vk_not_null<VkShaderModule> vkShader;
};
}

namespace cvk {
constexpr void Shader::debug_check(Shader const& shader) {
    CTH_CRITICAL(!shader.created(), "shader must be created") {}
}
}
