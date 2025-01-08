#pragma once
#include "src/vulkan/resource/buffer/CthBaseBuffer.hpp"

#include <volk.h>

#include <string>
#include <vector>


//TEMP modernize
namespace cth::vk {
struct DeviceTable;
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
     *@throws cth::vk::result_exception result of @ref vkCreateShaderModule()
     */
    explicit Shader(cth::not_null<Core const*> core, VkShaderStageFlagBits stage, std::string_view spv_path);
    /**
     *@throws cth::vk::result_exception result of @ref vkCreateShaderModule()
     */
    explicit Shader(cth::not_null<Core const*> core, VkShaderStageFlagBits stage, std::span<char const> spv);

    ~Shader();

    static void destroy(DeviceTable table, VkShaderModule vk_shader);

private:
    std::vector<char> loadSpv();
    void create(std::span<char const> spv);
    void destroy();
    void optDestroy() { if(created()) destroy(); }
    void reset();


    static std::string filename(std::string_view path) { return std::filesystem::path{path.data()}.filename().string(); }
#ifndef _FINAL
    void compile(std::string_view glsl_path, std::string_view compiler_path, std::string_view = "-O") const;
#endif

    cth::not_null<Core const*> _core;
    VkShaderStageFlagBits _vkStage;
    std::string _spvPath;

    move_ptr<VkShaderModule_T> _handle = VK_NULL_HANDLE;

public:
#ifndef _FINAL
    /**
    *@throws cth::vk::result_exception result of @ref vkCreateShaderModule()
    */
    explicit Shader(cth::not_null<Core const*> core, VkShaderStageFlagBits stages, std::string_view spv_path, std::string_view glsl_path,
        std::string_view compiler_path);
#endif
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkShaderModule module() const { return _handle.get(); }
    [[nodiscard]] VkShaderStageFlagBits stage() const { return _vkStage; }

    Shader(Shader const& other) = delete;
    Shader(Shader&& other) = delete;
    Shader& operator=(Shader const& other) = delete;
    Shader& operator=(Shader&& other) = delete;
};
} // namespace cth
