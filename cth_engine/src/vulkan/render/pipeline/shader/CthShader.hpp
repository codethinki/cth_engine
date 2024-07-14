#pragma once
#include "vulkan/resource/buffer/CthBasicBuffer.hpp"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>



namespace cth {
class Device;

struct ShaderSpecialization {
    ShaderSpecialization(std::span<VkSpecializationMapEntry> entries, std::span<char> data);

private:
    VkSpecializationInfo _vkInfo;

public:
    [[nodiscard]] const VkSpecializationInfo* get() const { return &_vkInfo; }
};

class Shader {
public:
    /**
     *@throws cth::except::vk_result_exception result of vkCreateShaderModule()
     */
    explicit Shader(const BasicCore* core, VkShaderStageFlagBits stage, std::string_view spv_path);
    /**
     *@throws cth::except::vk_result_exception result of vkCreateShaderModule()
     */
    explicit Shader(const BasicCore* core, VkShaderStageFlagBits stage, std::span<const char> spv);

    ~Shader();

private:
    std::vector<char> loadSpv();
    void create(std::span<const char> spv);

    static std::string filename(const std::string_view path) { return std::filesystem::path{path.data()}.filename().string(); }
#ifndef _FINAL
    void compile(std::string_view glsl_path, std::string_view compiler_path, std::string_view = "-O") const;
#endif

    const BasicCore* _core;
    VkShaderStageFlagBits _vkStage;
    std::string _spvPath;

    move_ptr<VkShaderModule_T> _handle = VK_NULL_HANDLE;

public:
#ifndef _FINAL
    /**
    *@throws cth::except::vk_result_exception result of vkCreateShaderModule()
    */
    explicit Shader(const BasicCore* core, VkShaderStageFlagBits stages, std::string_view spv_path, std::string_view glsl_path, std::string_view compiler_path);
#endif

    [[nodiscard]] VkShaderModule module() const { return _handle.get(); }
    [[nodiscard]] VkShaderStageFlagBits stage() const { return _vkStage; }

    Shader(const Shader& other) = delete;
    Shader(Shader&& other) = delete;
    Shader& operator=(const Shader& other) = delete;
    Shader& operator=(Shader&& other) = delete;
};
} // namespace cth
