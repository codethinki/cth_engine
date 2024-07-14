#pragma once
#include "HlcFrameInfo.hpp"

//TEMP for compile time speedup only include necessary headers
//TEMP replace this with #include <cth_engine/cth_engine.hpp>
#include <interface/render/model/CthVertex.hpp>
#include "vulkan/resource/buffer/CthBuffer.hpp"
#include "vulkan/resource/descriptor/descriptors/CthImageDescriptors.hpp"

#include <memory>


namespace cth {
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



inline constexpr std::string_view GLSL_COMPILER_PATH = R"(..\..\..\sdk\Vulkan\Bin\glslc.exe)";
inline constexpr std::string_view SHADER_GLSL_DIR = R"(src\render\glsl\)";
inline constexpr std::string_view SHADER_BINARY_DIR = R"(res\bin\shader\)";
inline constexpr std::string_view TEXTURE_DIR = R"(res\img\texture\)";

class RenderSystem {
public:
    RenderSystem(const BasicCore* core, DeletionQueue* deletion_queue, const PrimaryCmdBuffer& init_cmd_buffer, VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);
    ~RenderSystem() = default;

    void render(FrameInfo& frame_info) const;

private:
    void createShaders();
    void createDescriptorSetLayouts();

    void createPipelineLayout();
    void createPipeline(VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);

    void createDescriptorPool();

    void loadDescriptorData(const CmdBuffer& init_cmd_buffer, DeletionQueue* deletion_queue);

    void createDescriptorSets();

    //TEMP replace this with actual model loading
    void createDefaultTriangle(const CmdBuffer& cmd_buffer, DeletionQueue* deletion_queue);

    const BasicCore* _core;
    std::unique_ptr<PipelineLayout> _pipelineLayout;
    std::unique_ptr<Pipeline> _pipeline;

    std::unique_ptr<Shader> _vertexShader, _fragmentShader;

    //TEMP replaced with actual model data once ready
    std::unique_ptr<Buffer<Vertex>> _defaultTriangleBuffer{};

    std::unique_ptr<DescriptorSetLayout> _descriptorSetLayout;
    std::unique_ptr<DescriptorPool> _descriptorPool;
    std::unique_ptr<DescriptorSet> _descriptorSet;
    std::unique_ptr<TextureDescriptor> _textureDescriptor;

    std::unique_ptr<Texture> _texture;
    std::unique_ptr<ImageView> _textureView;
    std::unique_ptr<Sampler> _textureSampler;

public:
    RenderSystem(const RenderSystem& other) = delete;
    RenderSystem(RenderSystem&& other) noexcept = default;
    RenderSystem& operator=(const RenderSystem& other) = delete;
    RenderSystem& operator=(RenderSystem&& other) noexcept = default;
};
} // namespace cth
