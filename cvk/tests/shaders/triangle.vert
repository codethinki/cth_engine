#version 450

layout(location = 0) in vec3 position;

layout(push_constant) uniform Push {
    layout(offset = 16) mat4 viewProjection;
} push;

void main() { gl_Position = push.viewProjection * vec4(position, 1.0); }
