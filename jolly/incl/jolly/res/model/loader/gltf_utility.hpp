#pragma once
#include "jvk/utility/vk_exceptions.hpp"

#include <cth/macro.hpp>

#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include <cth/hash.hpp>

namespace jly {
enum class PipelineFlags : uint64_t {
    NONE = 0,

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

namespace dev {
    cxpr std::array<std::pair<std::string_view, PipelineFlags>, 7> ATTRIBUTE_MAPPINGS = {
        {
            {"NORMAL", PipelineFlags::NORMALS},
            {"TANGENT", PipelineFlags::TANGENTS},
            {"TEXCOORD_0", PipelineFlags::TEXCOORD_0},
            {"TEXCOORD_1", PipelineFlags::TEXCOORD_1},
            {"COLOR_0", PipelineFlags::COLOR_0},
            {"JOINTS_0", PipelineFlags::JOINTS_0},
            {"WEIGHTS_0", PipelineFlags::WEIGHTS_0},
        }
    };
}

cxpr PipelineFlags to_pipeline_flag(std::string_view const input) {
    for(auto const& [sv, flag] : dev::ATTRIBUTE_MAPPINGS)
        if(sv == input) return flag;

    JVK_STABLE_THROW(true, "input does not match any pipeline flag")
        details->add("input: {}", input);

    return PipelineFlags::NONE;
}
}

CTH_HASH_OVERLOAD(jly::PipelineFlags, jly::operator*)