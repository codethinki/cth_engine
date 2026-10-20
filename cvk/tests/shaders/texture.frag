#version 450

layout(set = 0, binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 outColor;

void main() { outColor = texture(tex, vec2(0.5)); }
