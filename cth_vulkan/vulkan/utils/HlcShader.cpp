#include "HlcShader.hpp"

#include <cth/cth_string.hpp>
#include <cth/cth_windows.hpp>

#include <cassert>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>



namespace cth {
vector<string> Shader::compile(const string& flags) const {
    constexpr string_view logFile = "shader_compile_log.txt";

    const string currentPath = filesystem::current_path().string();

   cth::win::cmd::hidden("del {0}", logFile.data());

    CTH_ERR(compilerPath.empty(), "empty compiler path") throw details->exception();

    const string command = std::format("\"{0}\"-c  \"{1}\" {2} -o \"{3}\">NUL 2>{4}",
        compilerPath.string(), glslPath.string(), flags, spvPath.string(), logFile.data());

    const int result = cth::win::cmd::hidden(command);

    CTH_STABLE_ERR(result != 0, "compile command failed") {
        details->add("command: \"{0}\"", command);
        throw details->exception();
    }

    auto debugInfo = cth::win::file::loadTxt<char>(format("{0}\\{1}", currentPath, logFile.data()));
    if(!debugInfo.empty()) debugInfo.resize(debugInfo.size() - 1);

    CTH_INFORM(debugInfo.empty(), std::format("compiled {0} shader", glslPath.filename().string()));


    for(auto& line : debugInfo) line = std::format("line{0}", line.substr(line.find(glslPath.filename().string())));

    return debugInfo;

}

void Shader::loadSpv() {
    std::ifstream file{spvPath, ios::ate | ios::binary};
    CTH_STABLE_ERR(!file.is_open(), "failed to open file") throw cth::except::data_exception{spvPath, details->exception()};

    const size_t fileSize = filesystem::file_size(spvPath);
    bytecode.resize(fileSize);
    file.read(bytecode.data(), static_cast<streamsize>(fileSize));
    file.close();

    CTH_STABLE_ERR(bytecode.empty(), "failed to load bytecode") throw cth::except::data_exception{spvPath, details->exception()};
}

void Shader::createModule(VkDevice device) {
    CTH_ERR(bytecode.empty(), "byte code for shader module missing") throw details->exception();
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytecode.size();
    createInfo.pCode = reinterpret_cast<uint32_t*>(bytecode.data());
    const VkResult createResult = vkCreateShaderModule(device, &createInfo, nullptr, &module);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "VK: failed to create shader module") throw details->exception();

    //TEMP left off here
    //BUG some memory leak somewhere (maybe the loading is not working)
    //BUG also fix the compile stuff

    wcout << glslPath.extension().wstring() << " shader Module created; size: " << bytecode.size() << " bytes" << endl;
    hasModule = true;
}
void Shader::destroyModule(VkDevice device) {
    vkDestroyShaderModule(device, module, nullptr);
    hasModule = false;
}
Shader::Shader(const filesystem::path& glsl_path, const filesystem::path& spv_path, const wstring& compiler_path) : glslPath(absolute(glsl_path)),
    spvPath{absolute(spv_path)}, compilerPath{compiler_path} {

    const wstring extension = glslPath.extension().wstring();
    if(extension == L".frag") type = TYPE_FRAGMENT;
    else if(extension == L".vert") type = TYPE_VERTEX;
    else
        CTH_ERR(true, "unknown shader type") throw details->exception();
}

}
