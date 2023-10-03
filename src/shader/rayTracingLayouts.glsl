#ifndef RAYTRACING_LAYOUTS_GLSL
#define RAYTRACING_LAYOUTS_GLSL

#include "layouts.glsl"

const float MinRayDistance = 1e-4;
const float MaxRayDistance = 1e7;

struct Intersection {
	vec2 bary;
	uint instanceIdx;
	uint triangleIdx;
	bool hit;
};

struct SurfaceInfo {
    vec3 pos;
    vec3 norm;
    vec3 albedo;
    int lightIndex;
    float transSurfaceArea;
    uint matIndex;
};

layout(set = RayTracingDescSet, binding = 0) uniform accelerationStructureEXT uTLAS;

void loadSurfaceInfo(Intersection isec, out SurfaceInfo info) {
    ObjectInstance instance = uObjectInstances[isec.instanceIdx];

    info.lightIndex = InvalidResourceIdx;
    info.matIndex = uMaterialIndices[instance.indexOffset / 3 + isec.triangleIdx];

    uint i0 = uIndices[instance.indexOffset + isec.triangleIdx * 3 + 0];
    uint i1 = uIndices[instance.indexOffset + isec.triangleIdx * 3 + 1];
    uint i2 = uIndices[instance.indexOffset + isec.triangleIdx * 3 + 2];

    MeshVertex v0 = uVertices[i0];
    MeshVertex v1 = uVertices[i1];
    MeshVertex v2 = uVertices[i2];

    vec3 bary = vec3(1.0 - isec.bary.x - isec.bary.y, isec.bary.x, isec.bary.y);;

    vec3 pos = v0.pos * bary.x + v1.pos * bary.y + v2.pos * bary.z;
    vec3 norm = v0.norm * bary.x + v1.norm * bary.y + v2.norm * bary.z;
    float uvx = v0.uvx * bary.x + v1.uvx * bary.y + v2.uvx * bary.z;
    float uvy = v0.uvy * bary.x + v1.uvy * bary.y + v2.uvy * bary.z;

    info.pos = vec3(instance.transform * vec4(pos, 1.0));
    info.norm = normalize(vec3(instance.transformInvT * vec4(norm, 1.0)));

    if (instance.lightIndex != InvalidResourceIdx) {
        info.albedo = uLightInstances[instance.lightIndex].radiance;
        info.transSurfaceArea = instance.transformedSurfaceArea;
        info.lightIndex = instance.lightIndex;
        return;
    }
    int texIdx = uMaterials[info.matIndex].textureIdx;

    if (texIdx == InvalidResourceIdx) {
        info.albedo = uMaterials[info.matIndex].baseColor;
    }
    else {
        info.albedo = texture(uTextures[nonuniformEXT(texIdx)], vec2(uvx, uvy)).rgb;
    }
    info.matIndex = info.matIndex;
}

#endif