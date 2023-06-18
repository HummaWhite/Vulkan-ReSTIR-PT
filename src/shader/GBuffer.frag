#version 460
#extension GL_GOOGLE_include_directive : enable

#include "layouts.glsl"

layout(location = 0) out vec4 DepthNormal;
layout(location = 1) out vec4 AlbedoMatIdx;

layout(location = 0) in VSOut {
	vec3 pos;
	float depth;
	vec3 norm;
	vec2 uv;
} fsIn;

void main() {
	vec3 albedo = (uDrawParam.matIdx == InvalidResourceIdx) ? fsIn.pos : uMaterials[uDrawParam.matIdx].baseColor;
	albedo = fsIn.pos;
    DepthNormal = vec4(fsIn.depth, fsIn.norm);
    AlbedoMatIdx = vec4(albedo, fsIn.uv.x);
}