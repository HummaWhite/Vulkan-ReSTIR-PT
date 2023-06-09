#version 460
#extension GL_GOOGLE_include_directive : enable

#include "HostDevice.h"

layout(location = 0) in vec3 aPos;
layout(location = 1) in float aTexX;
layout(location = 2) in vec3 aNorm;
layout(location = 3) in float aTexY;

layout(location = 0) out vec3 vPos;
layout(location = 1) out vec3 vNorm;
layout(location = 2) out vec2 vTexUV;

layout(set = 0, binding = 0) uniform _CameraBuffer {
	mat4 model;
	mat4 view;
	mat4 proj;
} uCamera;

void main() {
	gl_Position = uCamera.proj * uCamera.view * uCamera.model * vec4(aPos, 1.0);
	vPos = aPos;
	vNorm = aNorm;
	vTexUV = vec2(aTexX, aTexY);
}