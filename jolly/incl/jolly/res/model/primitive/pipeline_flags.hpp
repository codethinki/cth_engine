#pragma once

#include <cth/hash.hpp>

#include <cstdint>

namespace jly {
enum class PipelineFlags : uint64_t {
    // Vertex Attributes
    NORMALS = 1ULL << 0,
    TANGENTS = 1ULL << 1,
    TEXCOORD_0 = 1ULL << 2,
    TEXCOORD_1 = 1ULL << 3,
    COLOR_0 = 1ULL << 4,
    JOINTS_0 = 1ULL << 5,
    WEIGHTS_0 = 1ULL << 6,

    // Material Textures
    BASE_COLOR_TEXTURE = 1ULL << 10,
    METALLIC_ROUGHNESS_TEXTURE = 1ULL << 11,
    NORMAL_TEXTURE = 1ULL << 12,
    OCCLUSION_TEXTURE = 1ULL << 13,
    EMISSIVE_TEXTURE = 1ULL << 14,

    // Material Properties
    ALPHA_OPAQUE = 1ULL << 20,
    ALPHA_MASK = 1ULL << 21,
    ALPHA_BLEND = 1ULL << 22,
    DOUBLE_SIDED = 1ULL << 23,

    // Primitive Topology
    TOPOLOGY_POINTS = 1ULL << 30,
    TOPOLOGY_LINES = 1ULL << 31,
    TOPOLOGY_LINE_STRIP = 1ULL << 32,
    TOPOLOGY_TRIANGLES = 1ULL << 33,
    TOPOLOGY_TRIANGLE_STRIP = 1ULL << 34,
    TOPOLOGY_TRIANGLE_FAN = 1ULL << 35,
};

using base_pipeline_flags_t = std::underlying_type_t<PipelineFlags>;
cxpr base_pipeline_flags_t operator*(PipelineFlags val) { return static_cast<base_pipeline_flags_t>(val); }


cxpr PipelineFlags operator|(PipelineFlags l, PipelineFlags r) { return static_cast<PipelineFlags>(*l | *r); }

cxpr PipelineFlags operator&(PipelineFlags l, PipelineFlags r) { return static_cast<PipelineFlags>(*l & *r); }

cxpr PipelineFlags operator~(PipelineFlags val) { return PipelineFlags{(~*val)}; }

cxpr PipelineFlags& operator|=(PipelineFlags& l, PipelineFlags r) {
    l = l | r;
    return l;
}

cxpr PipelineFlags& operator&=(PipelineFlags& l, PipelineFlags r) {
    l = l & r;
    return l;
}
}

CTH_HASH_OVERLOAD(jly::PipelineFlags, jly::operator*)
