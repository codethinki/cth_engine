module;
#include "lib/volk.hpp"
export module demo.render.system;

import demo.render.frame_info;

import cth.engine.render;
import cth.vk;

import cth.ptr;

import std;


export namespace cth {
inline constexpr std::string_view GLSL_COMPILER_PATH = R"(..\..\..\sdk\Vulkan\Bin\glslc.exe)";
inline constexpr std::string_view SHADER_GLSL_DIR = R"(src\render\glsl\)";
inline constexpr std::string_view SHADER_BINARY_DIR = R"(res\bin\shader\)";
inline constexpr std::string_view TEXTURE_DIR = R"(res\img\texture\)";

class RenderSystem {
public:
    RenderSystem(vk::Core const* core, vk::PrimaryCmdBuffer const& init_cmd_buffer, vk::RenderPass const* render_pass,
        VkSampleCountFlagBits msaa_samples);
    ~RenderSystem() = default;

    void render(FrameInfo const& frame_info) const;

private:
    void createShaders();
    void createDescriptorSetLayouts();

    void createPipelineLayout();
    void createPipeline(VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);

    void createDescriptorPool();

    void loadDescriptorData(vk::CmdBuffer const& init_cmd_buffer);

    void createDescriptorSets();

    //TEMP replace this with actual model loading
    void createDefaultTriangle(vk::CmdBuffer const& cmd_buffer);

    cth::not_null<vk::Core const*> _core;
    std::unique_ptr<vk::PipelineLayout> _pipelineLayout;
    std::unique_ptr<vk::Pipeline> _pipeline;

    std::unique_ptr<vk::Shader> _vertexShader, _fragmentShader;

    //TEMP replaced with actual model data once ready
    std::unique_ptr<vk::Buffer<vk::Vertex>> _defaultTriangleBuffer{};

    std::unique_ptr<vk::DescriptorSetLayout> _descriptorSetLayout;
    std::unique_ptr<vk::DescriptorPool> _descriptorPool;
    std::unique_ptr<vk::DescriptorSet> _descriptorSet;
    std::unique_ptr<vk::TextureDescriptor> _textureDescriptor;

    std::unique_ptr<vk::Texture> _texture;
    std::unique_ptr<vk::ImageView> _textureView;
    std::unique_ptr<vk::Sampler> _textureSampler;

public:
    RenderSystem(RenderSystem const& other) = delete;
    RenderSystem(RenderSystem&& other) noexcept = default;
    RenderSystem& operator=(RenderSystem const& other) = delete;
    RenderSystem& operator=(RenderSystem&& other) noexcept = default;
};
} // namespace cth
