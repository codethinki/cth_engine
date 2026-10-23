#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform Push {
    uint index;
} push;

layout(location = 0) out vec4 outColor;

void main() { outColor = texture(textures[nonuniformEXT(push.index)], vec2(0.5)); }
