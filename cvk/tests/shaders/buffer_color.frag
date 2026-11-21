#version 450
#extension GL_EXT_buffer_reference : require

layout(buffer_reference, std430) readonly buffer Color {
    vec4 color;
};

layout(push_constant) uniform Push {
    Color color;
} push;

layout(location = 0) out vec4 outColor;

void main() { outColor = push.color.color; }
