#version 460
#extension GL_GOOGLE_include_directive : enable

#include "layouts.glsl"

layout(location = 0) in vec3 aPos;
layout(location = 1) in float aTexX;
layout(location = 2) in vec3 aNorm;
layout(location = 3) in float aTexY;

layout(location = 0) out VSOut {
	vec3 pos;
	float depth;
	vec3 norm;
	vec2 uv;
} vsOut;

void main() {
	GBufferDrawParam param = uGBufferDrawParams[gl_DrawID];
	gl_Position = uCamera.proj * uCamera.view * param.model * vec4(aPos, 1.0);
	vsOut.pos = aPos;
	vsOut.norm = aNorm;
	vsOut.uv = vec2(aTexX, aTexY);
	vsOut.depth = gl_Position.z / gl_Position.w;
}