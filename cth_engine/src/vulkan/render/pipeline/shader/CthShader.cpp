#include "CthShader.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

//Specialization

namespace cth::vk {
ShaderSpecialization::ShaderSpecialization(std::span<VkSpecializationMapEntry> entries, std::span<char> data) : _vkInfo{static_cast<uint32_t>(entries.size()),
    entries.data(), data.size(), reinterpret_cast<void*>(data.data())} {}
}

//Shader

namespace cth::vk {
Shader::Shader(const BasicCore* core, const VkShaderStageFlagBits stage, const std::string_view spv_path) : _core(core), _vkStage(stage), _spvPath(spv_path) {
    auto spv = loadSpv();
    create(spv);
}
Shader::Shader(const BasicCore* core, const VkShaderStageFlagBits stage, const std::span<const char> spv) : _core(core), _vkStage(stage) {
    create(spv);
}
Shader::~Shader() {
    vkDestroyShaderModule(_core->vkDevice(), _handle.get(), nullptr);
    log::msg("destroyed shader-module ({0})", filename(_spvPath));
}

std::vector<char> Shader::loadSpv() {
    CTH_STABLE_ERR(!std::filesystem::exists(_spvPath), "file does not exist") {
        details->add("file: {0}", _spvPath);
        throw details->exception();
    }


    std::ifstream file{_spvPath, std::ios::binary};
    CTH_STABLE_ERR(!file.is_open(), "failed to open file") {
        details->add("file: {0}", _spvPath);
        throw details->exception();
    }


    const size_t fileSize = std::filesystem::file_size(_spvPath);

    std::vector<char> bytecode(fileSize);
    file.read(bytecode.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    CTH_STABLE_ERR(bytecode.empty(), "failed to load bytecode") {
        details->add("file: {0}", _spvPath);
        throw details->exception();
    }

    cth::log::msg("loaded shader '{0}' ({1} bytes)", filename(_spvPath), fileSize);

    return bytecode;
}

void Shader::create(const std::span<const char> spv) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spv.size(); //size in bytes https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkShaderModuleCreateInfo.html
    createInfo.pCode = reinterpret_cast<const uint32_t*>(spv.data());

    VkShaderModule ptr = VK_NULL_HANDLE;

    const VkResult createResult = vkCreateShaderModule(_core->vkDevice(), &createInfo, nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create shader module")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    _handle = ptr;

    log::msg("created shader module ({0})", filename(_spvPath));
}




#ifndef _FINAL
void Shader::compile(const std::string_view glsl_path, const std::string_view compiler_path, const std::string_view flags) const {
    CTH_ERR(!std::filesystem::exists(compiler_path), "invalid compiler path") {
        details->add("path: {0}", compiler_path);
        throw details->exception();
    }
    CTH_ERR(!std::filesystem::exists(glsl_path), "invalid glsl path") {
        details->add("path: {0}", glsl_path);
        throw details->exception();
    }

    constexpr std::string_view logFile = "shader_compile_log.txt";

    const std::string command = std::format(R"("{0}" {1} -c "{2}" -o "{3}">NUL 2>"{4}")",
        compiler_path, flags, glsl_path, _spvPath, logFile);
    const int result = cth::win::cmd::hidden(command);

    std::vector<std::string> debugInfo = cth::io::loadTxt(logFile);

    if(debugInfo.empty()) {
        CTH_STABLE_ERR(result != 0, "compile command failed") {
            details->add("command: \"{0}\"", command);
            details->add("file: {}", filename(glsl_path));
            throw details->exception();
        }
        log::msg("compiled shader ({0})", filename(glsl_path));

        if(std::filesystem::exists(logFile)) std::filesystem::remove(logFile);
        return;
    }

    if(const auto name = filename(glsl_path); debugInfo.size() > 2) {
        debugInfo.resize(debugInfo.size() - 1);
        for(auto& line : debugInfo) line = std::format("line {}: ", line.substr(line.find(name) + name.size()));
    }
    CTH_STABLE_ABORT(true, "shader compilation failed") {
        details->add("file: {}", filename(glsl_path));
        details->add("{} errors:", debugInfo.size());
        for(auto& line : debugInfo)
            details->add("\t{}", line);
    }
}


Shader::Shader(const BasicCore* core, const VkShaderStageFlagBits stages, const std::string_view spv_path, const std::string_view glsl_path,
    const std::string_view compiler_path) : _core(core), _vkStage(stages),
    _spvPath{spv_path} {
#ifndef _DEBUG
    CTH_STABLE_WARN(true, "compiling shaders on startup, only use this on debug");
#endif

    compile(glsl_path, compiler_path);
    auto spv = loadSpv();
    create(spv);
}

#endif //_FINAL

}
