#include "CthShader.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


#include <filesystem>
#include <format>
#include <fstream>



namespace cth {
void Shader::loadSpv() {
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
    CTH_LOG(fileSize > 0, "loading shader") {
        details->add("file: {0}", filesystem::path(spvPath).filename().string());
        details->add("file size: {0} bytes", fileSize);
    }

    bytecode.resize(fileSize);
    file.read(bytecode.data(), static_cast<streamsize>(fileSize));
    file.close();

    CTH_STABLE_ERR(bytecode.empty(), "failed to load bytecode") {
        details->add("file: {0}", spvPath);
        throw details->exception();
    }
}

void Shader::create() {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytecode.size();
    createInfo.pCode = reinterpret_cast<uint32_t*>(bytecode.data());

    const VkResult createResult = vkCreateShaderModule(device->device(), &createInfo, nullptr, &vkModule);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create shader module")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    CTH_LOG(true, "created shader module: ") 
        details->add("file: {0}", filesystem::path(spvPath).filename().string());
}
void Shader::init() {
    loadSpv();
    create();
}



#ifndef _FINAL
void Shader::compile(const string_view flags) const {
    CTH_ERR(!filesystem::exists(compilerPath), "invalid compiler path") {
        details->add("path: {0}", compilerPath);
        throw details->exception();
    }
    CTH_ERR(!filesystem::exists(glslPath), "invalid glsl path") {
        details->add("path: {0}", glslPath);
        throw details->exception();
    }

    constexpr string_view logFile = "shader_compile_log.txt";
    const string glslFilename = filesystem::path(glslPath).filename().string();

    const string command = std::format(R"("{0}" {1} -c "{2}" -o "{3}">NUL 2>"{4}")",
        compilerPath, flags, glslPath, spvPath, logFile.data());
    //TEMP command not working fix this
    const int result = cth::win::cmd::hidden(command);

    vector<string> debugInfo = cth::win::file::loadTxt(logFile);

    if(debugInfo.empty()) {
        CTH_STABLE_ERR(result != 0, "compile command failed") {
            details->add("command: \"{0}\"", command);
            throw details->exception();
        }
        CTH_LOG(debugInfo.empty(), "compiled shader")
            details->add("file: {0}", glslFilename);

        if(filesystem::exists(logFile)) filesystem::remove(logFile);
        return;
    }

    debugInfo.resize(debugInfo.size() - 1);
    for(auto& line : debugInfo) line = std::format("line{0}", line.substr(line.find(glslFilename)));

    CTH_STABLE_ABORT(true, "shader compilation failed") {
        details->add("{} errors:", debugInfo.size());
        for(auto& line : debugInfo)
            details->add("\t{}", line);
    }
}

void Shader::checkExtension() const {
    const string extension = filesystem::path(glslPath).extension().string();
    Shader_Type shaderType = TYPES_SIZE;
    if(extension == ".frag") shaderType = TYPE_FRAGMENT;
    else if(extension == ".vert") shaderType = TYPE_VERTEX;

    CTH_ERR(shaderType != type, "shader type does not match with file extension") {
        details->add("extension: {}", extension);
        throw details->exception();
    }
}

Shader::Shader(Device* device, const Shader_Type type, const string_view spv_path, const string_view glsl_path,
    const string_view compiler_path) : device(device), type(type),
    spvPath{spv_path}, glslPath(glsl_path), compilerPath{compiler_path} {
#ifndef _DEBUG
    CTH_STABLE_WARN(true, "compiling shaders on startup, only use this on debug");
#endif

    checkExtension();

    compile();

    init();
}

#endif //_FINAL

Shader::Shader(Device* device, const Shader_Type type, const string_view spv_path) : device(device), type(type), spvPath(spv_path) {
    init();
}
Shader::~Shader() { vkDestroyShaderModule(device->device(), vkModule, nullptr); }

}

