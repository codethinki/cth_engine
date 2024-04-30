#pragma once
#include "HlcFrameInfo.hpp"

//TEMP for compile time speedup only include necessary headers
//TEMP replace this with #include <cth_engine/cth_engine.hpp>
#include <interface/render/model/CthVertex.hpp>
#include "vulkan/resource/buffer/CthBuffer.hpp"

#include <memory>


namespace cth {
using namespace std;
class Renderer;
class Device;
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



inline constexpr string_view GLSL_COMPILER_PATH = R"(..\..\..\sdk\Vulkan\Bin\glslc.exe)";
inline constexpr string_view SHADER_GLSL_DIR = R"(src\render\glsl\)";
inline constexpr string_view SHADER_BINARY_DIR = R"(res\bin\shader\)";
inline constexpr string_view TEXTURE_DIR = R"(res\img\texture\)";

class RenderSystem {
public:
    RenderSystem(Device* device, Renderer* renderer, VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);
    ~RenderSystem();

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
    void createDefaultTriangle(const CmdBuffer& cmd_buffer);

    Device* _device;
    Renderer* _renderer;
    unique_ptr<PipelineLayout> _pipelineLayout;
    unique_ptr<Pipeline> _pipeline;

    unique_ptr<Shader> _vertexShader, _fragmentShader;

    //TEMP replaced with actual model data once ready
    unique_ptr<Buffer<Vertex>> _defaultTriangleBuffer{};

    unique_ptr<DescriptorSetLayout> _descriptorSetLayout;
    unique_ptr<DescriptorPool> _descriptorPool;
    unique_ptr<DescriptorSet> _descriptorSet;
    unique_ptr<TextureDescriptor> _textureDescriptor;

    unique_ptr<Texture> _texture;
    unique_ptr<ImageView> _textureView;
    unique_ptr<Sampler> _textureSampler;

public:
    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;
};
} // namespace cth
