#pragma once
#include <cth/cth_algorithm.hpp>



#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <functional>


namespace cth {
using namespace std;



struct Vertex {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};

    constexpr Vertex() = default;
    constexpr Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& material_uv, const glm::vec2& uv) : position(position),
        normal(normal), uv(uv) {}

    constexpr bool operator==(const Vertex& other) const {
        return position == other.position && normal == other.normal && uv == other.uv;
    }

    constexpr Vertex(const Vertex& other) = default;
    constexpr Vertex& operator=(const Vertex& other) = default;

    //IMPLEMENT the per instance data field


};
inline constexpr array<VkVertexInputBindingDescription, 1> VERTEX_BINDING_DESCRIPTIONS{{
    {0, static_cast<uint32_t>(sizeof(Vertex)), VK_VERTEX_INPUT_RATE_VERTEX}
}};
inline constexpr array<VkVertexInputAttributeDescription, 3> VERTEX_ATTRIBUTE_DESCRIPTIONS{{
    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, position))},
    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal))},
    {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, uv))},
}};


using vertex_t = Vertex;
using index_t = uint32_t;
using instance_t = int; //TEMP change this to something

template<typename T>
inline constexpr bool is_render_type_v = _Is_any_of_v<T, vertex_t, index_t, instance_t>;

template<typename T>
concept render_type = is_render_type_v<T>;





}


template<>
struct std::hash<cth::Vertex> {
    size_t operator()(cth::Vertex const& vertex) const {
        size_t seed = 0;
        cth::algorithm::hash::combine(seed, vertex.position, vertex.normal, vertex.uv);
        return seed;
    }
};
