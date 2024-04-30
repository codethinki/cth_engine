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
public:
    /**
     *\throws cth::except::vk_result_exception result of vkCreateShaderModule()
     */
    explicit Shader(Device* device, VkShaderStageFlagBits stage, string_view spv_path);
    /**
     *\throws cth::except::vk_result_exception result of vkCreateShaderModule()
     */
    explicit Shader(Device* device, VkShaderStageFlagBits stage, span<const char> spv);

    ~Shader();

private:
    vector<char> loadSpv();
    void create(span<const char> spv);

    static string filename(const string_view path) { return filesystem::path{path.data()}.filename().string(); }
#ifndef _FINAL
    void compile(string_view glsl_path, string_view compiler_path, string_view = "-O") const;
#endif

    Device* device;
    VkShaderStageFlagBits vkStage;
    string spvPath;

    VkShaderModule vkModule = VK_NULL_HANDLE;

public:
#ifndef _FINAL
    /**
    *\throws cth::except::vk_result_exception result of vkCreateShaderModule()
    */
    explicit Shader(Device* device, VkShaderStageFlagBits stage, string_view spv_path, string_view glsl_path, string_view compiler_path);
#endif

    [[nodiscard]] VkShaderModule module() const { return vkModule; }
    [[nodiscard]] VkShaderStageFlagBits stage() const { return vkStage; }

    Shader(const Shader& other) = delete;
    Shader(Shader&& other) = delete;
    Shader& operator=(const Shader& other) = delete;
    Shader& operator=(Shader&& other) = delete;
};
} // namespace cth
