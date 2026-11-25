#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "imgui.glsl"

// the color attachment has an sRGB format: it encodes the linear output
layout(constant_id = 0) const bool SRGB_TARGET = false;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUv;

// the sRGB colors of ImGui to linear
vec3 to_linear(vec3 srgb) {
    bvec3 low = lessThanEqual(srgb, vec3(0.04045));
    return mix(pow((srgb + 0.055) / 1.055, vec3(2.4)), srgb / 12.92, low);
}

void main() {
    // gl_VertexIndex includes the vertex offset of the draw
    Vertex vertex = draw.vertices.vertices[gl_VertexIndex];
    vec4 color = unpackUnorm4x8(vertex.color);
    if(SRGB_TARGET)
        color.rgb = to_linear(color.rgb);

    outColor = color;
    outUv = vec2(vertex.u, vertex.v);
    gl_Position = vec4(vec2(vertex.x, vertex.y) * draw.scale + draw.translate, 0.0, 1.0);
}
