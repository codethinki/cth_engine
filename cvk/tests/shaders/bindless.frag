#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 1) uniform sampler samp;

layout(push_constant) uniform Push {
    uint textureIdx;
} push;

layout(location = 0) out vec4 outColor;

void main() { outColor = texture(sampler2D(textures[nonuniformEXT(push.textureIdx)], samp), vec2(0.5)); }
