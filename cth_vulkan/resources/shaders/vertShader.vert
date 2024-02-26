#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 inColor;

layout(location = 0)  out vec4 outColor;

layout(push_constant) uniform Push{mat4 modelMatrix; vec4 color;} push;
layout(set =  0, binding = 0) uniform UniformBuffer{mat4 viewMatrix; } ubo;

void main(){	
	vec4 positionW = push.modelMatrix * vec4(position, 1);
	gl_Position =  ubo.viewMatrix * positionW;
	outColor = push.color;
}