#version 460
#extension GL_GOOGLE_include_directive : enable

#include "layouts.glsl"

layout(location = 0) in vec3 aPos;
layout(location = 1) in float aTexX;
layout(location = 2) in vec3 aNorm;
layout(location = 3) in float aTexY;

layout(location = 0) out VSOut {
	vec3 pos;
	vec3 norm;
	vec2 uv;
	flat uint instanceIdx;
} vsOut;

void main() {
	ObjectInstance instance = uObjectInstances[gl_InstanceIndex];

	vec4 pos = instance.transform * vec4(aPos, 1.0);
	gl_Position = uCamera.projView * pos;
	
	vsOut.pos = pos.xyz;
	vsOut.norm = normalize(mat3(instance.transformInvT) * aNorm);
	vsOut.uv = vec2(aTexX, aTexY);
	vsOut.instanceIdx = gl_InstanceIndex;
}