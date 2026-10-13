#version 450

layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

void main(){
	outColor = vec4(texture(texSampler, uv).xyz, 1);

}
