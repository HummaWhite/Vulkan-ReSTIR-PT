#version 460
#extension GL_GOOGLE_include_directive : enable

#include "HostDevice.h"

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vTexCoord;

layout(set = 0, binding = 0) uniform _CameraBuffer {
	float pad0;
	vec3 pad2;

	mat4 model;
	mat4 view;
	mat4 proj;
} cam;

void main() {
	gl_Position = cam.proj * cam.view * cam.model * vec4(aPos, 1.0);
	vColor = aColor;
	vTexCoord = aTexCoord;
}