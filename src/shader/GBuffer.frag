#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

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
	float alpha = 1.0;

	if (param.matIdx == InvalidResourceIdx) {
		albedo = fsIn.norm * 0.5 + 0.5;
	}
	else {
		int texIdx = uMaterials[param.matIdx].textureIdx;

		if (texIdx == InvalidResourceIdx) {
			albedo = uMaterials[param.matIdx].baseColor;
		}
		else {
			vec4 albedoAlpha = texture(uTextures[texIdx], fsIn.uv);
			albedo = albedoAlpha.rgb;
			alpha = albedoAlpha.a;
		}
	}

	vec4 lastCoord = uCamera.lastProjView * vec4(fsIn.pos, 1.0);
	lastCoord /= lastCoord.w;
	lastCoord.y = -lastCoord.y;
	lastCoord = lastCoord * 0.5 + 0.5;

	vec2 thisCoord = gl_FragCoord.xy / uCamera.filmSize;

	if (alpha < 0.5) {
		discard;
	}

	packGBuffer(
		GBufferA, GBufferB,
		albedo, fsIn.norm, fsIn.depth,
		lastCoord.xy - thisCoord, param.matIdx
	);
}