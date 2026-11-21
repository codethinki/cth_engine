#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include <jvk/heaps.glsl>
#include "cube.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColor;

const vec3 TO_LIGHT = normalize(vec3(0.4, 0.8, 0.6));
const float AMBIENT = 0.15;

void main() {
    vec3 albedo = texture(sampler2D(jvk_textures2D[draw.textureIdx], jvk_samplers[draw.samplerIdx]), inUv).rgb;
    float lambert = max(dot(normalize(inNormal), TO_LIGHT), 0.0);
    outColor = vec4(albedo * (AMBIENT + (1.0 - AMBIENT) * lambert), 1.0);
}
