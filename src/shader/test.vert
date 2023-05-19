#version 460
#extension GL_GOOGLE_include_directive : enable

#include "HostDevice.h"

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

layout(location = 0) out vec3 vColor;


layout(binding = 0) uniform _CameraBuffer {
	mat4 model;
	mat4 view;
	mat4 proj;
} cam;

void main() {
	gl_Position = cam.proj * cam.view * cam.model * vec4(aPos, 1.0);
	//gl_Position = vec4(aPos, 1.0);
	vColor = aColor;
}