#pragma once
#include "HlcFrameInfo.hpp"
#include  "../core/CthDevice.hpp"
#include "../core/CthPipeline.hpp"
#include "../memory/buffer/CthBuffer.hpp"

#include <memory>
#include <vector>

#include "../models/HlcVertex.hpp"


namespace cth {
class MemoryManager;
using namespace std;

class RenderObject;

class RenderSystem {
public:
    struct RenderData {
        explicit RenderData(vector<unique_ptr<RenderObject>>& objects) : objects(objects) {}

        array<size_t, 4> groupSizes{};

        vector<uint32_t> groupIndices{};
        vector<unique_ptr<RenderObject>>& objects;
    };

    RenderSystem(Device* device, VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);
    ~RenderSystem();

    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    VkExtent2D windowSize = {1000, 1000};

    //TEMP remove this
    void createDefaultTriangle();
    void render(FrameInfo& frame_info) const;

private:
    //void initSamplers();
    void initDescriptorUtils();
    void initDescriptorSets();
    void initDescriptedBuffers(vector<VkDescriptorBufferInfo>& buffer_infos);
    //void initDescriptedTextures(uint32_t& descriptor_set_index, vector<VkDescriptorImageInfo>& image_infos);

    void createPipelineLayout();
    void createPipeline(VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);

    Device* hlcDevice;
    //TEMP implement this

    //MemoryManager& memoryManager;

    //unique_ptr<CthDescriptorPool> descriptorPool{};
    //unique_ptr<HlcDescriptorSetLayout> descriptorSetLayout{};
    //unique_ptr<HlcTextureSampler> defaultTextureSampler;
    //vector<VkDescriptorSet> descriptorSets;
    //vector<unique_ptr<DefaultBuffer> descriptedBuffers;

    //vector<unique_ptr<Image>> descriptedImages;
    unique_ptr<Pipeline> hlcPipeline;
    VkPipelineLayout pipelineLayout{};


    //TEMP remove this
    unique_ptr<Buffer<Vertex>> defaultTriangleBuffer{};
};
}
