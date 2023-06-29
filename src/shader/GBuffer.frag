#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "layouts.glsl"
#include "GBufferUtil.glsl"

layout(location = 0) out uvec4 GBufferA;
layout(location = 1) out uvec4 GBufferB;

layout(location = 0) in VSOut {
	vec3 pos;
	float depth;
	vec3 norm;
	vec2 uv;
} fsIn;

void main() {
	GBufferDrawParam param = uGBufferDrawParam;
	vec3 albedo;

	if (param.matIdx == InvalidResourceIdx) {
		albedo = fsIn.norm * 0.5 + 0.5;
	}
	else {
		int texIdx = uMaterials[param.matIdx].textureIdx;

		if (texIdx == InvalidResourceIdx) {
			albedo = uMaterials[param.matIdx].baseColor;
		}
		else {
			albedo = texture(uTextures[texIdx], fsIn.uv).rgb;
		}
	}
	albedo = albedo * vec3(-dot(fsIn.norm, uCamera.front) * 0.5 + 0.55);

	vec4 lastCoord = uCamera.lastProjView * vec4(fsIn.pos, 1.0);
	lastCoord /= lastCoord.w;
	lastCoord = lastCoord * 0.5 + 0.5;

	packGBuffer(
		GBufferA, GBufferB,
		albedo, fsIn.norm, fsIn.depth,
		lastCoord.xy - gl_FragCoord.xy, param.matIdx
	);
}