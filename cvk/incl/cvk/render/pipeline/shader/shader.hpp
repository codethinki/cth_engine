#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <tuple>
#include <vector>


namespace cvk {
class Core;

/**
 * `VkShaderModule` wrapper
 */
class Shader {
public:
    struct State;
    using path_t = std::filesystem::path;

    /**
     * base constructor
     * @param stage of the shader
     */
    Shader(Core const&, VkShaderStageFlagBits stage);

    /**
     * constructs and wraps
     * @details calls @ref Shader(Core const&, VkShaderStageFlagBits), `wrap()`
     */
    Shader(Core const&, VkShaderStageFlagBits stage, State const&);

    /**
     * constructs and creates
     * @details calls @ref Shader(Core const&, VkShaderStageFlagBits), @ref create(std::span<uint32_t const>)
     */
    Shader(Core const&, VkShaderStageFlagBits stage, std::span<uint32_t const> spv);

    /**
     * constructs, loads and creates
     * @details calls @ref Shader(Core const&, VkShaderStageFlagBits), @ref create(path_t const&)
     */
    Shader(Core const&, VkShaderStageFlagBits stage, path_t const& spvPath);

    /**
     * @note calls `optDestroy()`
     */
    ~Shader() { optDestroy(); }

    /**
     * wraps the state
     * @pre `Core::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the module
     * @param spv SPIR-V code
     * @pre `Core::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateShaderModule()`
     */
    void create(std::span<uint32_t const> spv);

    /**
     * loads the SPIR-V file and creates the module
     * @details calls `load()`, @ref create(std::span<uint32_t const>)
     * @pre `Core::created()`
     */
    void create(path_t const& spvPath);

    /**
     * destroys and forgets
     * @pre `created()`
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    /**
     * @return stage info for a pipeline
     * @param specializationPtr `nullptr` -> no specialization constants
     * @param entryPoint name of the entry function
     * @pre `created()`
     * @attention `specializationPtr` and `entryPoint` must outlive the pipeline creation
     */
    [[nodiscard]] VkPipelineShaderStageCreateInfo stageInfo(
        VkSpecializationInfo const* specializationPtr = nullptr,
        char const* entryPoint = "main"
    ) const;

    /**
     * reads a SPIR-V file
     * @throws cvk::cvk_exception reason: file missing, size not a multiple of 4 or 0
     */
    [[nodiscard]] static std::vector<uint32_t> load(path_t const& spvPath);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkShaderModule get() const { return _handle.get(); }
    [[nodiscard]] constexpr VkShaderStageFlagBits stage() const { return _stage; }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }

private:
    void forget();

    core_handle<VkShaderModule> _handle;
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

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkShaderModule> decompose() && { return {vkShader.get()}; }
};
}

namespace cvk {
constexpr void Shader::debug_check(Shader const& shader) {
    CTH_CRITICAL(!shader.created(), "shader must be created") {}
}
}
