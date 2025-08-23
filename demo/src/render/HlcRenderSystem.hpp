#pragma once
#include "HlcFrameInfo.hpp"

//TEMP for compile time speedup only include necessary headers
//TEMP replace this with #include <cth_engine/cth_engine.hpp>


#include "jolly/render/model/CthVertex.hpp"

#include "jvk/res/buffer/buffer.hpp"

#include <memory>


namespace jvk {
class RenderPass;
class Sampler;
class ImageView;
class Texture;
class TextureDescriptor;
class DescriptorSet;
class DescriptorPool;
class DescriptorSetLayout;
class Shader;
class Pipeline;
class PipelineLayout;
}

namespace cth {

/**
 * @brief expects glslc to be in path
 */
//inline constexpr std::string_view GLSL_COMPILER_PATH = R"(glslc.exe)";
//inline constexpr std::string_view SHADER_GLSL_DIR = R"(src\render\glsl\)";
inline constexpr std::string_view SHADER_BINARY_DIR = R"(demo_shaders\)";
inline constexpr std::string_view TEXTURE_DIR = R"(res\img\texture\)";

class RenderSystem {
public:
    RenderSystem(jvk::Core const* core, jvk::PrimaryCmdBuffer const& init_cmd_buffer,
        jvk::RenderPass const& render_pass, VkSampleCountFlagBits msaa_samples);
    ~RenderSystem();

    void render(FrameInfo const& frame_info) const;

private:
    void createShaders();
    void createDescriptorSetLayouts();

    void createPipelineLayout();
    void createPipeline(VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);

    void createDescriptorPool();
    void loadDescriptorData(jvk::CmdBuffer const& init_cmd_buffer);


    void createDescriptorSets();

    //TEMP replace this with actual model loading
    void createDefaultTriangle(jvk::CmdBuffer const& cmd_buffer);

    not_null<jvk::Core const*> _core;
    std::unique_ptr<jvk::PipelineLayout> _pipelineLayout;
    std::unique_ptr<jvk::Pipeline> _pipeline;

    std::unique_ptr<jvk::Shader> _vertexShader, _fragmentShader;

    //TEMP replaced with actual model data once ready
    std::unique_ptr<jvk::Buffer<jvk::Vertex>> _defaultTriangleBuffer{};

    std::unique_ptr<jvk::DescriptorSetLayout> _descriptorSetLayout;
    std::unique_ptr<jvk::DescriptorPool> _descriptorPool;
    std::unique_ptr<jvk::DescriptorSet> _descriptorSet;
    std::unique_ptr<jvk::TextureDescriptor> _textureDescriptor;

    std::unique_ptr<jvk::Texture> _texture;
    std::unique_ptr<jvk::ImageView> _textureView;
    std::unique_ptr<jvk::Sampler> _textureSampler;

public:
    RenderSystem(RenderSystem const& other) = delete;
    RenderSystem(RenderSystem&& other) noexcept = default;
    RenderSystem& operator=(RenderSystem const& other) = delete;
    RenderSystem& operator=(RenderSystem&& other) noexcept = default;
};
} // namespace cth
