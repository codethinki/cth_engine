#include "cvk/render/pipeline/shader/shader.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/io/file.hpp>
#include <cth/os/osdef.hpp>

#include <cstdlib>
#include <format>
#include <fstream>


namespace cvk {
Shader::Shader(Core const& core, VkShaderStageFlagBits stage) : _core{&core}, _stage{stage} {
    Core::debug_check(core);
}

Shader::Shader(Core const& core, VkShaderStageFlagBits stage, State const& state) : Shader{core, stage} {
    wrap(state);
}

Shader::Shader(Core const& core, VkShaderStageFlagBits stage, std::span<uint32_t const> spv) : Shader{core, stage} {
    create(spv);
}

Shader::Shader(Core const& core, VkShaderStageFlagBits stage, path_t const& spvPath) : Shader{core, stage} {
    create(spvPath);
}

Shader::Shader(
    Core const& core,
    VkShaderStageFlagBits stage,
    path_t const& spvPath,
    path_t const& glslPath,
    path_t const& compilerPath
) : Shader{core, stage} {
    CTH_STABLE_WARN(!constants::DEBUG_MODE, "compiles shaders at run time, use this in debug builds only") {}

    compile(glslPath, spvPath, compilerPath);
    create(spvPath);
}

void Shader::wrap(State const& state) {
    optDestroy();

    _handle = state.vkShader.get();
}

void Shader::create(std::span<uint32_t const> spv) {
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
    auto const result = _core->functions()->vkCreateShaderModule(_core->vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create shader module") {
        invalidate();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle = ptr;
}

void Shader::create(path_t const& spvPath) {
    auto const spv = load(spvPath);
    create(spv);
}

void Shader::destroy() {
    debug_check(*this);

    destroy(_core->deviceTable(), _handle.get());

    invalidate();
}

Shader::State Shader::release() {
    debug_check(*this);

    State const state{_handle.get()};
    invalidate();
    return state;
}

VkPipelineShaderStageCreateInfo Shader::stageInfo(
    VkSpecializationInfo const* specialization,
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
        .pSpecializationInfo = specialization,
    };
}

std::vector<uint32_t> Shader::load(path_t const& spvPath) {
    CVK_STABLE_THROW(!std::filesystem::exists(spvPath), "SPIR-V file does not exist") {
        details->add("file: {}", spvPath.string());
    }

    auto const size = std::filesystem::file_size(spvPath);
    CVK_STABLE_THROW(size == 0 || size % sizeof(uint32_t) != 0, "SPIR-V file size must be a multiple of 4") {
        details->add("file: {}, size: {}", spvPath.string(), size);
    }

    std::vector<uint32_t> spv(size / sizeof(uint32_t));

    std::ifstream file{spvPath, std::ios::binary};
    file.read(reinterpret_cast<char*>(spv.data()), static_cast<std::streamsize>(size));
    CVK_STABLE_THROW(!file, "failed to read SPIR-V file") { details->add("file: {}", spvPath.string()); }

    return spv;
}

void Shader::compile(
    path_t const& glslPath,
    path_t const& spvPath,
    path_t const& compilerPath,
    std::string_view flags
) {
    CVK_STABLE_THROW(!std::filesystem::exists(glslPath), "glsl file does not exist") {
        details->add("file: {}", glslPath.string());
    }

#ifdef CTH_PLATFORM_WINDOWS
    constexpr std::string_view nullDevice = "NUL";
#else
    constexpr std::string_view nullDevice = "/dev/null";
#endif
    auto const logName = std::format("cvk_{}.log", spvPath.filename().string());
    auto const logPath = std::filesystem::temp_directory_path() / logName;

    auto command = std::format(
        R"("{}" {} -c "{}" -o "{}" >{} 2>"{}")",
        compilerPath.string(),
        flags,
        glslPath.string(),
        spvPath.string(),
        nullDevice,
        logPath.string()
    );
#ifdef CTH_PLATFORM_WINDOWS
    // cmd.exe /c removes the outer quotes of a command that starts with a quote
    command = std::format("\"{}\"", command);
#endif

    int const result = std::system(command.c_str());

    std::vector<std::string> output{};
    if(std::filesystem::exists(logPath)) {
        output = cth::io::file::chop(logPath);
        std::erase_if(output, [](std::string const& line) { return line.empty(); });
        std::filesystem::remove(logPath);
    }

    CVK_STABLE_THROW(result != 0, "shader compilation failed") {
        details->add("file: {}", glslPath.string());
        details->add("command: {}", command);
        for(auto const& line : output) details->add("{}", line);
    }

    CTH_STABLE_WARN(!output.empty(), "shader compiler output") {
        details->add("file: {}", glslPath.string());
        for(auto const& line : output) details->add("{}", line);
    }
}

void Shader::destroy(DeviceTable table, VkShaderModule vkShader) {
    CTH_WARN(vkShader == VK_NULL_HANDLE, "vkShader invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyShaderModule(table.device(), vkShader, nullptr);
}

void Shader::invalidate() { _handle = VK_NULL_HANDLE; }
}
