// fragment shader: set 0 binding 0 sampled image, binding 1 sampler, location 0 color output of its
// center
#version 450

layout(set = 0, binding = 0) uniform texture2D tex;
layout(set = 0, binding = 1) uniform sampler samp;

layout(location = 0) out vec4 outColor;

void main() { outColor = texture(sampler2D(tex, samp), vec2(0.5)); }
