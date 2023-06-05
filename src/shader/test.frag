#version 460

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vTexCoord;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
	FragColor = texture(texSampler, vTexCoord);
}