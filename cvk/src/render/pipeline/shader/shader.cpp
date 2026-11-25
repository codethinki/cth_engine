#include "cvk/render/pipeline/shader/shader.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <filesystem>
#include <fstream>


namespace cvk {
Shader::Shader(Core const& core, VkShaderStageFlagBits stage) : _handle{core}, _stage{stage} {}

Shader::Shader(Core const& core, VkShaderStageFlagBits stage, State const& state) : Shader{
    core,
    stage
} { wrap(state); }

Shader::Shader(Core const& core, VkShaderStageFlagBits stage, std::span<uint32_t const> spv) :
    Shader{
        core,
        stage
    } { create(spv); }

Shader::Shader(Core const& core, VkShaderStageFlagBits stage, path_t const& spvPath) : Shader{
    core,
    stage
} { create(spvPath); }

void Shader::wrap(State const& state) {
    Core::debug_check(core());

    optDestroy();

    _handle.reset(state.vkShader.get(), core());
}

void Shader::create(std::span<uint32_t const> spv) {
    Core::debug_check(core());
    CTH_CRITICAL(spv.empty(), "spv must not be empty") {}

    optDestroy();

    VkShaderModuleCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = spv.size_bytes(),
        .pCode = spv.data(),
    };

    VkShaderModule ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateShaderModule(
        core().vkDevice(),
        &info,
        nullptr,
        &ptr
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create shader module") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void Shader::create(path_t const& spvPath) {
    auto const spv = load(spvPath);
    create(spv);
}

void Shader::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

Shader::State Shader::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

VkPipelineShaderStageCreateInfo Shader::stageInfo(
    VkSpecializationInfo const* specializationPtr,
    char const* entryPoint
) const {
    debug_check(*this);

    return VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = _stage,
        .module = _handle.get(),
        .pName = entryPoint,
        .pSpecializationInfo = specializationPtr,
    };
}

std::vector<uint32_t> Shader::load(path_t const& spvPath) {
    CVK_STABLE_THROW(
        !std::filesystem::exists(spvPath),
        "SPIR-V file {} does not exist",
        spvPath.string()
    ) {}

    auto const size = std::filesystem::file_size(spvPath);
    CVK_STABLE_THROW(
        size == 0 || size % sizeof(uint32_t) != 0,
        "SPIR-V file size must be a multiple of 4, got {} for {}",
        size,
        spvPath.string()
    ) {}

    std::vector<uint32_t> spv(size / sizeof(uint32_t));

    std::ifstream file{spvPath, std::ios::binary};
    file.read(reinterpret_cast<char*>(spv.data()), static_cast<std::streamsize>(size));
    CVK_STABLE_THROW(!file, "failed to read SPIR-V file {}", spvPath.string()) {}

    return spv;
}

void destroy(DeviceTable table, VkShaderModule vkShader) {
    CTH_WARN(vkShader == VK_NULL_HANDLE, "vkShader invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyShaderModule(table.device(), vkShader, nullptr);
}

void Shader::forget() {}
}
