#include "jvk/render/pipeline/shader/shader.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/device_table.hpp"
#include "jvk/res/destruction_queue.hpp"
#include "jvk/utility/vk_exceptions.hpp"

#include <cth/windows.hpp>
#include <cth/io/file.hpp>

namespace dev {
static std::string to_string(std::filesystem::path const& path) { return path.string(); }
}

CTH_FORMAT_TYPE(std::filesystem::path, dev::to_string);

//Specialization

namespace jvk {
ShaderSpecialization::ShaderSpecialization(std::span<VkSpecializationMapEntry> entries,
    std::span<char> data) : _vkInfo{
    static_cast<uint32_t>(entries.size()),
    entries.data(), data.size(), reinterpret_cast<void*>(data.data())} {}
}

//Shader

namespace jvk {
Shader::Shader(Core const& core, VkShaderStageFlagBits stage, path_t spv_path) : _core{&core},
    _vkStage{stage}, _spvPath{std::move(spv_path)} {
    auto spv = loadSpv();
    create(spv);
}

Shader::Shader(Core const& core, VkShaderStageFlagBits stage, std::span<char const> spv) : _core{&core},
    _vkStage{stage} { create(spv); }

Shader::~Shader() { optDestroy(); }

void Shader::destroy(DeviceTable table, VkShaderModule vk_shader) {
    CTH_WARN(vk_shader == VK_NULL_HANDLE, "vk_shader should not be invalid (VK_NULL_HANDLE)") {}

    log::msg("destroyed shader-module ({0})", reinterpret_cast<void*>(vk_shader));

    table->vkDestroyShaderModule(table.device(), vk_shader, nullptr);
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


    size_t const fileSize = std::filesystem::file_size(_spvPath);

    std::vector<char> bytecode(fileSize);
    file.read(bytecode.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    CTH_STABLE_ERR(bytecode.empty(), "failed to load bytecode") {
        details->add("file: {0}", _spvPath);
        throw details->exception();
    }

    cth::log::msg("loaded shader '{0}' ({1} bytes)", _spvPath, fileSize);

    return bytecode;
}

void Shader::create(std::span<char const> spv) {
    VkShaderModuleCreateInfo const createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spv.size(),
        //size in bytes https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkShaderModuleCreateInfo.html
        .pCode = reinterpret_cast<uint32_t const*>(spv.data()),
    };

    VkShaderModule ptr = VK_NULL_HANDLE;

    VkResult const createResult = _core->functions()->vkCreateShaderModule(_core->vkDevice(), &createInfo,
        nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create shader module")
    throw jvk::result_exception{createResult, details->exception()};

    _handle = ptr;

    log::msg("created shader module ({0})", _spvPath);
}

void Shader::destroy() {
    CTH_CRITICAL(!created(), "requires created()") {}

    auto const queue = _core->destructionQueue();

    auto const lambda = [table = _core->deviceTable(), shader = _handle.get()] { destroy(table, shader); };

    if(queue) queue->push(lambda);
    else lambda();

    reset();
}

void Shader::reset() { _handle = VK_NULL_HANDLE; }



#ifndef _FINAL
void Shader::compile(path_t const& glsl_path, path_t const& compiler_path, std::string_view flags) const {

    CTH_STABLE_ERR(!std::filesystem::exists(glsl_path), "invalid glsl path") {
        details->add("path: {0}", glsl_path);
        throw details->exception();
    }

    constexpr std::string_view logFile = "shader_compile_log.txt";

    std::string const command = std::format(R"("{0}" {1} -c "{2}" -o "{3}">NUL 2>"{4}")",
        compiler_path, flags, glsl_path, _spvPath, logFile);
    int const result = cth::win::cmd::hidden(command);

    std::vector<std::string> debugInfo = cth::io::file::chop(logFile);

    auto const glslFile = glsl_path.filename().string();

    if(debugInfo.empty()) {
        CTH_STABLE_ERR(result != 0, "compile command failed") {
            details->add("command: \"{0}\"", command);
            details->add("file: {}", glslFile);
            throw details->exception();
        }
        log::msg("compiled shader ({0})", glslFile);

        if(std::filesystem::exists(logFile)) std::filesystem::remove(logFile);
        return;
    }

    if(debugInfo.size() > 2) {
        debugInfo.resize(debugInfo.size() - 1);
        for(auto& line : debugInfo) line = std::format("line {}: ",
            line.substr(line.find(glslFile) + glslFile.size()));
    }
    CTH_STABLE_ABORT(true, "shader compilation failed") {
        details->add("file: {}", glslFile);
        details->add("{} errors:", debugInfo.size());
        for(auto& line : debugInfo)
            details->add("\t{}", line);
    }
}


Shader::Shader(Core const& core, VkShaderStageFlagBits stages, path_t spv_path, path_t const& glsl_path,
    path_t const& compiler_path) : _core{&core}, _vkStage{stages},
    _spvPath{std::move(spv_path)} {
#ifndef _DEBUG
    CTH_STABLE_WARN(true, "compiling shaders on startup, only use this on debug") {}
#endif

    compile(glsl_path, compiler_path);
    auto spv = loadSpv();
    create(spv);
}

#endif //_FINAL

}