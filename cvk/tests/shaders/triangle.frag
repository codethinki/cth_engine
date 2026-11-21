#version 450

layout(push_constant) uniform Push {
    vec4 color;
} push;

layout(location = 0) out vec4 outColor;

void main() { outColor = push.color; }
