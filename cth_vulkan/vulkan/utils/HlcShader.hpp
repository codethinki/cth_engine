#pragma once

#include "..\core\CthWindow.hpp"
#include <string>
#include <filesystem>

#include <vector>


namespace cth {
using namespace std;

class Shader {
public:
    enum Shader_Type { TYPE_FRAGMENT, TYPE_VERTEX, TYPES_SIZE };

    [[nodiscard]] vector<wstring> compile(const wstring& flags = L"-O") const;

    void loadSpv();

    void createModule(VkDevice device);
    void destroyModule(VkDevice device);


    [[nodiscard]] vector<char> getBytecode() const { return bytecode; }
    [[nodiscard]] VkShaderModule getModule() const { return module; }
    /**
     *@note shader class for vulkan glsl shaders
     *@param glsl_path path to the glsl code file
     *@param spv_path path to the SPIR-V bytecode file
     *@param compiler_path path to the glsl compiler
     */
    Shader(const filesystem::path& glsl_path, const filesystem::path& spv_path, const wstring& compiler_path = L"");

private:
    Shader_Type type;
    filesystem::path glslPath, spvPath, compilerPath;
    vector<char> bytecode;
    VkShaderModule module{};
    bool hasModule = false;
};
}
