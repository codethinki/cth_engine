#include "CthShader.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


#include <filesystem>
#include <format>
#include <fstream>

//Specialization

namespace cth {
ShaderSpecialization::ShaderSpecialization(span<VkSpecializationMapEntry> entries, span<char> data) : vkInfo{static_cast<uint32_t>(entries.size()),
    entries.data(), data.size(), reinterpret_cast<void*>(data.data())} {}
}

//Shader

namespace cth {
Shader::Shader(Device* device, const VkShaderStageFlagBits stage, const string_view spv_path) : device(device), vkStage(stage), spvPath(spv_path) {
    auto spv = loadSpv();
    create(spv);
}
Shader::Shader(Device* device, const VkShaderStageFlagBits stage, const span<const char> spv) : device(device), vkStage(stage) {
    create(spv);
}
Shader::~Shader() {
    vkDestroyShaderModule(device->get(), vkModule, nullptr);
    log::msg("destroyed shader-module ({0})", filename(spvPath));
}

vector<char> Shader::loadSpv() {
    CTH_STABLE_ERR(!filesystem::exists(spvPath), "file does not exist") {
        details->add("file: {0}", spvPath);
        throw details->exception();
    }


    std::ifstream file{spvPath, ios::binary};
    CTH_STABLE_ERR(!file.is_open(), "failed to open file") {
        details->add("file: {0}", spvPath);
        throw details->exception();
    }


    const size_t fileSize = filesystem::file_size(spvPath);

    vector<char> bytecode(fileSize);
    file.read(bytecode.data(), static_cast<streamsize>(fileSize));
    file.close();

    CTH_STABLE_ERR(bytecode.empty(), "failed to load bytecode") {
        details->add("file: {0}", spvPath);
        throw details->exception();
    }

    cth::log::msg("loaded shader '{0}' ({1} bytes)", filename(spvPath), fileSize);

    return bytecode;
}

void Shader::create(const span<const char> spv) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spv.size(); //size in bytes https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkShaderModuleCreateInfo.html
    createInfo.pCode = reinterpret_cast<const uint32_t*>(spv.data());

    const VkResult createResult = vkCreateShaderModule(device->get(), &createInfo, nullptr, &vkModule);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create shader module")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    log::msg("created shader module ({0})", filename(spvPath));
}




#ifndef _FINAL
void Shader::compile(const string_view glsl_path, const string_view compiler_path, const string_view flags) const {
    CTH_ERR(!filesystem::exists(compiler_path), "invalid compiler path") {
        details->add("path: {0}", compiler_path);
        throw details->exception();
    }
    CTH_ERR(!filesystem::exists(glsl_path), "invalid glsl path") {
        details->add("path: {0}", glsl_path);
        throw details->exception();
    }

    constexpr string_view logFile = "shader_compile_log.txt";

    const string command = std::format(R"("{0}" {1} -c "{2}" -o "{3}">NUL 2>"{4}")",
        compiler_path, flags, glsl_path, spvPath, logFile);
    //TEMP command not working fix this
    const int result = cth::win::cmd::hidden(command);

    vector<string> debugInfo = cth::win::file::loadTxt(logFile);

    if(debugInfo.empty()) {
        CTH_STABLE_ERR(result != 0, "compile command failed") {
            details->add("command: \"{0}\"", command);
            details->add("file: {}", filename(glsl_path));
            throw details->exception();
        }
        log::msg("compiled shader ({0})", filename(glsl_path));

        if(filesystem::exists(logFile)) filesystem::remove(logFile);
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


Shader::Shader(Device* device, const VkShaderStageFlagBits stage, const string_view spv_path, const string_view glsl_path,
    const string_view compiler_path) : device(device), vkStage(stage),
    spvPath{spv_path} {
#ifndef _DEBUG
    CTH_STABLE_WARN(true, "compiling shaders on startup, only use this on debug");
#endif

    compile(glsl_path, compiler_path);
    auto spv = loadSpv();
    create(spv);
}

#endif //_FINAL

}
