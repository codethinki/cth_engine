#pragma once
#include "HlcFrameInfo.hpp"

//TEMP for compile time speedup only include necessary headers
//TEMP replace this with #include <cth_engine/cth_engine.hpp>
#include <interface/render/model/CthVertex.hpp>
#include "vulkan/resource/buffer/CthBuffer.hpp"
#include "vulkan/resource/descriptor/descriptors/CthImageDescriptors.hpp"

#include <memory>


namespace cth {
namespace vk {
    class Texture;
    class Shader;
    class PipelineLayout;
    class Pipeline;
    class DescriptorSetLayout;
    class DescriptorPool;
    class DescriptorSet;
    class TextureDescriptor;
    class ImageView;
    class Texture;
    class Sampler;
}



inline constexpr std::string_view GLSL_COMPILER_PATH = R"(..\..\..\sdk\Vulkan\Bin\glslc.exe)";
inline constexpr std::string_view SHADER_GLSL_DIR = R"(src\render\glsl\)";
inline constexpr std::string_view SHADER_BINARY_DIR = R"(res\bin\shader\)";
inline constexpr std::string_view TEXTURE_DIR = R"(res\img\texture\)";

class RenderSystem {
public:
    RenderSystem(vk::BasicCore const* core, vk::DestructionQueue* destruction_queue, vk::PrimaryCmdBuffer const& init_cmd_buffer, VkRenderPass render_pass,
        VkSampleCountFlagBits msaa_samples);
    ~RenderSystem() = default;

    void render(FrameInfo& frame_info) const;

private:
    void createShaders();
    void createDescriptorSetLayouts();

    void createPipelineLayout();
    void createPipeline(VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);

    void createDescriptorPool();

    void loadDescriptorData(vk::CmdBuffer const& init_cmd_buffer, vk::DestructionQueue* destruction_queue);

    void createDescriptorSets();

    //TEMP replace this with actual model loading
    void createDefaultTriangle(vk::CmdBuffer const& cmd_buffer, vk::DestructionQueue* destruction_queue);

    vk::BasicCore const* _core;
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
