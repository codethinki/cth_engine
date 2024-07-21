#pragma once
#include <cth/cth_algorithm.hpp>



#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <functional>


namespace cth::vk {

struct Vertex {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};

    constexpr Vertex() = default;
    constexpr Vertex(glm::vec3 const& position, glm::vec3 const& normal, glm::vec2 const& uv) : position(position),
        normal(normal), uv(uv) {}

    constexpr bool operator==(Vertex const& other) const {
        return position == other.position && normal == other.normal && uv == other.uv;
    }

    constexpr Vertex(Vertex const& other) = default;
    constexpr Vertex& operator=(Vertex const& other) = default;

    //IMPLEMENT the per instance data field
};
inline constexpr std::array<VkVertexInputBindingDescription, 1> VERTEX_BINDING_DESCRIPTIONS{{
    {0, static_cast<uint32_t>(sizeof(Vertex)), VK_VERTEX_INPUT_RATE_VERTEX}
}};
inline constexpr std::array<VkVertexInputAttributeDescription, 3> VERTEX_ATTRIBUTE_DESCRIPTIONS{{
    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, position))},
    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal))},
    {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, uv))},
}};


using vertex_t = Vertex;
using index_t = uint32_t;
using instance_t = int; //TEMP change this to something

template<typename T>
inline constexpr bool is_render_type_v = std::_Is_any_of_v<T, vertex_t, index_t, instance_t>;

template<typename T>
concept render_type = is_render_type_v<T>;

}


template<>
struct std::hash<cth::vk::Vertex> {
    size_t operator()(cth::vk::Vertex const& vertex) const {
        size_t seed = 0;
        cth::algorithm::hash::combine(seed, vertex.position, vertex.normal, vertex.uv);
        return seed;
    }
};
