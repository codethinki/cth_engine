// vertex shader: location 0 `vec3` position, no descriptors
#version 450

layout(location = 0) in vec3 position;

void main() { gl_Position = vec4(position, 1.0); }
