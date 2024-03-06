#include "HlcShader.hpp"

#include <cth/cth_string.hpp>
#include <cth/cth_windows.hpp>

#include <cassert>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>



namespace cth {
vector<string> Shader::compile(const wstring& flags) const {
    using filesystem::absolute;
    const wstring currentPath = filesystem::current_path();

    cth::win::cmd::hidden(L"del shaderCompileLog.txt");


    CTH_ERR(compilerPath.empty(), "empty compiler path") throw cth::except::data_exception{compilerPath, details->exception()};

    const string command = std::format(R"("{0}" {1} "{2}" -o "{3}">NUL 2>shader_compile_log.txt)",
        compilerPath, flags, glslPath, spvPath);

    const int result = cth::win::cmd::hidden(command);

    CTH_STABLE_ERR(result == -1, "compile command failed") throw cth::except::data_exception{command, details->exception()};

    auto debugInfo = cth::win::file::loadTxt<string>(format(L"{0}\\shaderCompileLog.txt", currentPath));

    if(debugInfo.empty()) {
        cth::win::cmd::hidden(L"del shaderCompileLog.txt");
        wcout << "compiled " << glslPath << '\n';
        return vector<string>{};
    }

    //TODO add proper error output
    //error output
    /*debugInfo.replace(0, debugInfo.find(L':', debugInfo.find(L':') + 1), L"");
    wcout << shader_path.filename() << L" error in line "
        << debugInfo.substr(debugInfo.find(L':') + 1, debugInfo.find(L':', debugInfo.find(L':') + 1) - debugInfo.find(L':')) << '\n';
    wcout << debugInfo.replace(0, debugInfo.find(L"error:"), L"") << '\n';*/

    return debugInfo;
}

void Shader::loadSpv() {
    std::ifstream file{spvPath, ios::ate | ios::binary};
    CTH_STABLE_ERR(!file.is_open(), "failed to open file") throw cth::except::data_exception{spvPath, details->exception()};

    const size_t fileSize = filesystem::file_size(spvPath);
    bytecode.resize(fileSize);
    file.read(bytecode.data(), static_cast<streamsize>(fileSize));
    file.close();

    CTH_STABLE_ERR(!bytecode.empty(), "failed to load bytecode") throw cth::except::data_exception{spvPath, details->exception()};
}

void Shader::createModule(VkDevice device) {
    CTH_ERR(!bytecode.empty(), "byte code for shader module missing") throw details->exception();
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytecode.size();
    createInfo.pCode = reinterpret_cast<uint32_t*>(bytecode.data());
    CTH_STABLE_ERR(vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS, "VK: failed to create shader module")
        throw details->
            exception();

    //TEMP left off here

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
