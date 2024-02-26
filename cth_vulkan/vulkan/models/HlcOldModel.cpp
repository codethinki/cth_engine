#include "HlcOldModel.hpp"

#include <glm/gtx/hash.hpp>

#include <cassert>
#include <cstring>


namespace cth {
using namespace std;


void OldModel::draw(VkCommandBuffer command_buffer) const {
    if(hasIndexBuffer) vkCmdDrawIndexed(command_buffer, indexCount, 1, 0, 0, 0);
    else vkCmdDraw(command_buffer, vertexCount, 1, 0, 0);
}

void OldModel::bind(VkCommandBuffer command_buffer) const {
    const VkBuffer buffers[] = {vertexBuffer->getBuffer()};
    constexpr VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, buffers, offsets);
    if(hasIndexBuffer) vkCmdBindIndexBuffer(command_buffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void OldModel::createVertexBuffers(const std::vector<Vertex>& vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");
    vertexBuffer = make_unique<Buffer>(hlcDevice,
        sizeof(vertices[0]),
        static_cast<uint32_t>(vertices.size()),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vertexBuffer->stage(vertices.data());
}

void OldModel::createIndexBuffers(const std::vector<uint32_t>& indices) {
    indexCount = static_cast<uint32_t>(indices.size());
    if(indexCount > 0) hasIndexBuffer = true;
    else return;

    indexBuffer = make_unique<Buffer>(hlcDevice,
        sizeof(indices[0]),
        static_cast<uint32_t>(indices.size()),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    indexBuffer->stage(indices.data());
}

std::unique_ptr<OldModel> OldModel::createSquareModel(Device& device, const glm::vec3 position, const glm::vec2 extent) {
    vector<Vertex> vertices{{
        Vertex{{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}},
        Vertex{{extent.x, extent.y, 0.f}, {1.f, 1.f, 0.f}},
        Vertex{{0.f, extent.y, 0.f}, {0.f, 1.f, 0.f}},
        Vertex{{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}},
        Vertex{{extent.x, 0.f, 0.f}, {0.f, 0.f, 1.f}},
        Vertex{{extent.x, extent.y, 0.f}, {1.f, 1.f, 0.f}},
    }};

    if(position.x != 0.f || position.y != 0.f) for(auto& [vPosition, color] : vertices) { vPosition += position; }
    return std::make_unique<OldModel>(device, Builder{vertices});
}

void OldModel::regModel(const string& model_name, unique_ptr<OldModel> model) {
    assert(!models.contains(model_name) && "regModel: model is already registered");
    if(model_name[0] == '@') models[model_name.substr(1, model_name.size() - 1)] = std::move(model);
    else models[model_name] = std::move(model);
}

OldModel* OldModel::model(const string& model_name) {
    assert(models.contains(model_name) && "model: model is not registered");
    return models[model_name].get();
}

void OldModel::delModel(const string& model_name) {
    assert(models.contains(model_name) && "delModel: model doesn't exist");
    models.erase(model_name);
}

OldModel::OldModel(Device& device, const Builder& builder) : hlcDevice{device} {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
}

OldModel::~OldModel() = default;


OldModel::Builder::Builder(const vector<Vertex>& obj_vertices) {
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for(auto& vertex : obj_vertices) {
        if(!uniqueVertices.contains(vertex)) {
            uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
            vertices.push_back(vertex);
        }
        indices.push_back(uniqueVertices[vertex]);
    }
}
}
