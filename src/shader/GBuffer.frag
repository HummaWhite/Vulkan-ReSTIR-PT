#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : enable

#include "layouts.glsl"
#include "gbuffer_util.glsl"

layout(location = 0) out vec4 DepthNormal;
layout(location = 1) out uvec2 AlbedoMatId;
layout(location = 2) out vec2 MotionVector;

layout(location = 0) in VSOut {
	vec3 pos;
	vec3 norm;
	vec2 uv;
	flat uint instanceIdx;
	flat uint indexOffset;
} fsIn;

void main() {
	uint matIndex = uMaterialIndices[fsIn.indexOffset / 3 + gl_PrimitiveID];

	Material mat = uMaterials[matIndex];

	vec3 albedo;
	float alpha = 1.0;

	uint texIdx = mat.textureIdx;

	if (texIdx == InvalidResourceIdx) {
		albedo = mat.baseColor;
	}
	else {
		vec4 albedoAlpha = texture(uTextures[texIdx], fsIn.uv);
		albedo = albedoAlpha.rgb;
		alpha = albedoAlpha.a;
	}

	vec4 lastCoord = uCamera.lastProjView * vec4(fsIn.pos, 1.0);
	lastCoord /= lastCoord.w;
	lastCoord = lastCoord * 0.5 + 0.5;

	vec2 thisCoord = gl_FragCoord.xy / uCamera.filmSize;

	if (alpha < 0.5) {
		//discard;
	}
	uint meshIdx = fsIn.instanceIdx;

	DepthNormal = vec4(length(uCamera.pos - fsIn.pos), normalize(fsIn.norm));
	AlbedoMatId = uvec2(packAlbedo(albedo), uint(matIndex << 16 | meshIdx));
	MotionVector = lastCoord.xy - thisCoord;
}