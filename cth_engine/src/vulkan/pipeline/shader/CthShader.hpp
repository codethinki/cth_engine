#pragma once
#include <cth/cth_windows.hpp>

#include <vulkan/vulkan.h>

#include <string>
#include <vector>


namespace cth {
using namespace std;
class Device;

struct ShaderSpecialization {
    ShaderSpecialization(span<VkSpecializationMapEntry> entries, span<char> data);

private:
    VkSpecializationInfo vkInfo;

public:
    [[nodiscard]] const VkSpecializationInfo* get() const { return &vkInfo; }
};

class Shader {
    void loadSpv();
    void create();

    void init();
#ifndef _FINAL
    void compile(string_view = "-O") const;
#endif

    Device* device;
    VkShaderStageFlagBits vkStage;
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
    explicit Shader(Device* device, VkShaderStageFlagBits stage, string_view spv_path, string_view glsl_path, string_view compiler_path);
#endif
    /**
     *\throws cth::except::vk_result_exception result of vkCreateShaderModule()
     */
    explicit Shader(Device* device, VkShaderStageFlagBits stage, string_view spv_path);
    ~Shader();


    [[nodiscard]] vector<char> binary() const { return bytecode; }
    [[nodiscard]] size_t size() const { return bytecode.size(); }
    [[nodiscard]] VkShaderModule module() const { return vkModule; }
    [[nodiscard]] VkShaderStageFlagBits stage() const { return vkStage; }

    Shader(const Shader& other) = delete;
    Shader(Shader&& other) = delete;
    Shader& operator=(const Shader& other) = delete;
    Shader& operator=(Shader&& other) = delete;
};
}
