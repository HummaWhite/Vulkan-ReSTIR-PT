#version 460

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNorm;
layout(location = 2) in vec2 vTexUV;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
	FragColor = vec4(vNorm, 1.0);
	//FragColor = texture(texSampler, vTexUV);
}