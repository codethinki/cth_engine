#version 450

layout(set = 0, binding = 0) uniform Color {
    vec4 color;
} ubo;

layout(location = 0) out vec4 outColor;

void main() { outColor = ubo.color; }
