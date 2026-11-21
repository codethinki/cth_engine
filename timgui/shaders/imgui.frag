#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include <jvk/heaps.glsl>
#include "imgui.glsl"

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texel = texture(sampler2D(jvk_textures2D[draw.textureIdx], jvk_samplers[draw.samplerIdx]), inUv);
    outColor = inColor * texel;
}
