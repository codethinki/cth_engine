#pragma once
#include <cth/cth_windows.hpp>

#include <vulkan/vulkan.h>

#include <string>
#include <vector>


namespace cth {
using namespace std;
class Device;

class Shader {
public:
    enum Shader_Type { TYPE_FRAGMENT, TYPE_VERTEX, TYPES_SIZE };

private:
    void loadSpv();
    void create();

    void init();
#ifndef _FINAL
    void checkExtension() const;

    void compile(string_view = "-O") const;
#endif

    Device* device;
    Shader_Type type;
    string spvPath;

    vector<char> bytecode;
    VkShaderModule vkModule = VK_NULL_HANDLE;

#ifndef _FINAL
    string glslPath, compilerPath;
#endif

public:
#ifndef _FINAL
    /**
    *\throws cth::except::vk_result_exception result of vkCreateShaderModule()
    */
    explicit Shader(Device* device, Shader_Type type, string_view spv_path, string_view glsl_path, string_view compiler_path);
#endif
    /**
     *\throws cth::except::vk_result_exception result of vkCreateShaderModule()
     */
    explicit Shader(Device* device, Shader_Type type, string_view spv_path);
    ~Shader();


    [[nodiscard]] vector<char> binary() const { return bytecode; }
    [[nodiscard]] size_t size() const { return bytecode.size(); }
    [[nodiscard]] VkShaderModule get() const { return vkModule; }

    Shader(const Shader& other) = delete;
    Shader(Shader&& other) = delete;
    Shader& operator=(const Shader& other) = delete;
    Shader& operator=(Shader&& other) = delete;
};
}
