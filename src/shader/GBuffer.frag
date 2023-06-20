#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

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

	albedo = albedo * vec3(abs(dot(fsIn.norm, uCamera.front)) + 0.05);
    DepthNormal = vec4(fsIn.depth, fsIn.norm);
    AlbedoMatIdx = vec4(albedo, fsIn.uv.x);
}