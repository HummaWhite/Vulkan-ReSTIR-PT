#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "rayTracingLayouts.glsl"
#include "math.glsl"
#include "material.glsl"
#include "lightSampling.glsl"

layout(location = 0) rayPayloadInEXT Intersection rtIsec;

hitAttributeEXT vec3 attribs;

void main() {
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    ObjectInstance instance = uObjectInstances[gl_InstanceCustomIndexEXT];

    rtIsec.hit = true;
    rtIsec.bary = bary;
    rtIsec.instanceIndex = gl_InstanceCustomIndexEXT;
    rtIsec.lightIndex = InvalidResourceIdx;

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
    rtIsec.pos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));
    rtIsec.norm = normalize(transpose(mat3(gl_WorldToObjectEXT)) * norm);
    rtIsec.uv = vec2(uvx, uvy);

    if (instance.lightIndex != InvalidResourceIdx) {
        rtIsec.albedo = uLightInstances[instance.lightIndex].radiance;
        rtIsec.transSurfaceArea = instance.transformedSurfaceArea;
        rtIsec.lightIndex = instance.lightIndex;
        return;
    }

	int texIdx = uMaterials[matIdx].textureIdx;

	if (texIdx == InvalidResourceIdx) {
	    albedo = uMaterials[matIdx].baseColor;
	}
	else {
	    albedo = texture(uTextures[texIdx], vec2(uvx, uvy)).rgb;
	}
    rtIsec.matIndex = matIdx;
    rtIsec.texIndex = texIdx;
    rtIsec.albedo = albedo;
}
