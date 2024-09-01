#pragma once
#include "vulkan/resource/buffer/CthBasicBuffer.hpp"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>



namespace cth::vk {
class Device;

struct ShaderSpecialization {
    ShaderSpecialization(std::span<VkSpecializationMapEntry> entries, std::span<char> data);

private:
    VkSpecializationInfo _vkInfo;

public:
    [[nodiscard]] VkSpecializationInfo const* get() const { return &_vkInfo; }
};

class Shader {
public:
    /**
     *@throws cth::except::vk_result_exception result of vkCreateShaderModule()
     */
    explicit Shader(not_null<BasicCore const*> core, VkShaderStageFlagBits stage, std::string_view spv_path);
    /**
     *@throws cth::except::vk_result_exception result of vkCreateShaderModule()
     */
    explicit Shader(not_null<BasicCore const*> core, VkShaderStageFlagBits stage, std::span<char const> spv);

    ~Shader();

private:
    std::vector<char> loadSpv();
    void create(std::span<char const> spv);

    static std::string filename(std::string_view path) { return std::filesystem::path{path.data()}.filename().string(); }
#ifndef _FINAL
    void compile(std::string_view glsl_path, std::string_view compiler_path, std::string_view = "-O") const;
#endif

    not_null<BasicCore const*> _core;
    VkShaderStageFlagBits _vkStage;
    std::string _spvPath;

    move_ptr<VkShaderModule_T> _handle = VK_NULL_HANDLE;

public:
#ifndef _FINAL
    /**
    *@throws cth::except::vk_result_exception result of vkCreateShaderModule()
    */
    explicit Shader(not_null<BasicCore const*> core, VkShaderStageFlagBits stages, std::string_view spv_path, std::string_view glsl_path, std::string_view compiler_path);
#endif

    [[nodiscard]] VkShaderModule module() const { return _handle.get(); }
    [[nodiscard]] VkShaderStageFlagBits stage() const { return _vkStage; }

    Shader(Shader const& other) = delete;
    Shader(Shader&& other) = delete;
    Shader& operator=(Shader const& other) = delete;
    Shader& operator=(Shader&& other) = delete;
};
} // namespace cth