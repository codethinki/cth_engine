#pragma once
#include "HlcFrameInfo.hpp"

//TEMP for compile time speedup only include necessary headers
//TEMP replace this with #include <cth_engine/cth_engine.hpp>
#include <vulkan/render/model/CthVertex.hpp>
#include "vulkan/memory/buffer/CthBuffer.hpp"

#include <memory>


namespace cth {
using namespace std;
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
    void render(FrameInfo& frame_info) const;

private:
    void createShaders();
    void createDescriptorSetLayouts();

    void createPipelineLayout();
    void createPipeline(VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);

    void createDescriptorPool();

    void loadDescriptorData();

    void createDescriptorSets();
    
    //TEMP replace this with actual model loading
    void createDefaultTriangle();

    Device* device;
    unique_ptr<PipelineLayout> pipelineLayout;
    unique_ptr<Pipeline> pipeline;

    unique_ptr<Shader> vertexShader, fragmentShader;

    //TEMP replaced with actual model data once ready
    unique_ptr<Buffer<Vertex>> defaultTriangleBuffer{};

    unique_ptr<DescriptorSetLayout> descriptorSetLayout;
    unique_ptr<DescriptorPool> descriptorPool;
    unique_ptr<DescriptorSet> descriptorSet;
    unique_ptr<TextureDescriptor> textureDescriptor;

    unique_ptr<Texture> texture;
    unique_ptr<ImageView> textureView;
    unique_ptr<Sampler> textureSampler;

public:
    RenderSystem(Device* device, VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);
    ~RenderSystem();

    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;
};
} // namespace cth

//TEMP clean this up
//public
//void initDescriptedTextures(uint32_t& descriptor_set_index, vector<VkDescriptorImageInfo>& image_infos);

//private
//void initSamplers();
//void initDescriptorUtils();
//void initDescriptorSets();
//void initDescriptedBuffers(vector<VkDescriptorBufferInfo>& buffer_infos);

//MemoryManager& memoryManager;

//unique_ptr<CthDescriptorPool> descriptorPool{};
//unique_ptr<HlcDescriptorSetLayout> descriptorSetLayout{};
//unique_ptr<HlcTextureSampler> defaultTextureSampler;
//vector<VkDescriptorSet> descriptorSets;
//vector<unique_ptr<DefaultBuffer> descriptedBuffers;

//vector<unique_ptr<Image>> descriptedImages;
