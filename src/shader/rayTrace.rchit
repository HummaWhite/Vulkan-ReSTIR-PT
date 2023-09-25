#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "rayTracingLayouts.glsl"
#include "math.glsl"

layout(location = 0) rayPayloadInEXT RTPayload rtPayload;
hitAttributeEXT vec3 attribs;

vec3 sampleLight(vec3 ref, out float pdf, inout uint rng) {
    return vec3(0.f);
}

vec3 sampleLights(vec3 ref, out float pdf, inout uint rng) {
    float sum = uLightSampleTable[0].prob;
    uint num = uLightSampleTable[0].failId;

    int idx = 1;
    vec3 radiance = sampleLight(ref, pdf, rng);
    return vec3(0.f);
}

void main() {
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    ObjectInstance instance = uObjectInstances[gl_InstanceCustomIndexEXT];

    uint i0 = uIndices[instance.indexOffset + gl_PrimitiveID * 3 + 0];
    uint i1 = uIndices[instance.indexOffset + gl_PrimitiveID * 3 + 1];
    uint i2 = uIndices[instance.indexOffset + gl_PrimitiveID * 3 + 2];

    int matIdx = uMaterialIndices[instance.indexOffset / 3 + gl_PrimitiveID];

    MeshVertex v0 = uVertices[i0];
    MeshVertex v1 = uVertices[i1];
    MeshVertex v2 = uVertices[i2];

    vec3 pos = v0.pos * bary.x + v1.pos * bary.y + v2.pos * bary.z;
    vec3 norm = v0.norm * bary.x + v1.norm * bary.y + v2.norm * bary.z;
    float uvx = v0.uvx * bary.x + v1.uvx * bary.y + v2.uvx * bary.z;
    float uvy = v0.uvy * bary.x + v1.uvy * bary.y + v2.uvy * bary.z;
    vec3 albedo;

    // pos = vec3(instance.transform * vec4(pos, 1.0));
    // norm = normalize(vec3(instance.transformInvT * vec4(norm, 1.0)));
    pos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));
    norm = normalize(transpose(mat3(gl_WorldToObjectEXT)) * norm);

    if (matIdx == InvalidResourceIdx) {
		    albedo = norm * 0.5 + 0.5;
	  }
	  else {
		    int texIdx = uMaterials[matIdx].textureIdx;

		    if (texIdx == InvalidResourceIdx) {
			      albedo = uMaterials[matIdx].baseColor;
		    }
		    else {
			      albedo = texture(uTextures[texIdx], vec2(uvx, uvy)).rgb;
		    }
	  }
	  albedo = albedo * vec3(-dot(norm, uCamera.front) * 0.5 + 0.55);

    rtPayload.radiance = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    rtPayload.radiance = pos;
    rtPayload.radiance = norm * 0.5 + 0.5;
    rtPayload.radiance = albedo;
}
