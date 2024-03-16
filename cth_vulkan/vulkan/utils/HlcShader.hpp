#pragma once

#include "../core/CthWindow.hpp"
#include <string>
#include <filesystem>

#include <vector>
#include <cth/cth_windows.hpp>

//TEMP left off here refactor this
namespace cth {
using namespace std;

class Shader {
public:
    enum Shader_Type { TYPE_FRAGMENT, TYPE_VERTEX, TYPES_SIZE };

    /**
     * \brief 
     * \return debug info
     */
    [[nodiscard]] vector<string> compile(const string& flags = "-O") const;

    void loadSpv();

    void createModule(VkDevice device);
    void destroyModule(VkDevice device);


    [[nodiscard]] vector<char> binary() const { return bytecode; }
    [[nodiscard]] size_t size(){return bytecode.size();}
    [[nodiscard]] VkShaderModule get() const { return module; }


    explicit Shader(string_view spv_path, string_view  glsl_path = "", string_view compiler_path = "");

private:
    Shader_Type type;

    const string spvPath, glslPath, compilerPath;

    vector<char> bytecode;
    VkShaderModule module{};
    bool hasModule = false;
};
}
