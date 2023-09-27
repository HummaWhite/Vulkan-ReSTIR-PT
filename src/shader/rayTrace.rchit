#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "rayTracingLayouts.glsl"
#include "math.glsl"
#include "lightSampling.glsl"
#include "material.glsl"

layout(location = 0) rayPayloadInEXT RTPayload rtPayload;
layout(location = 1) rayPayloadEXT RTShadowPayload rtShadowPayload;

hitAttributeEXT vec3 attribs;

vec3 sampleLight(ObjectInstance obj, vec3 ref, out float dist, out vec3 norm, vec3 r) {
    uint primCount = obj.indexCount / 3;
    uint primIdx = uint(float(primCount) * r.x);

    uint i0 = uIndices[obj.indexOffset + primIdx * 3 + 0];
    uint i1 = uIndices[obj.indexOffset + primIdx * 3 + 1];
    uint i2 = uIndices[obj.indexOffset + primIdx * 3 + 2];

    vec3 v0 = uVertices[i0].pos;
    vec3 v1 = uVertices[i1].pos;
    vec3 v2 = uVertices[i2].pos;

    vec3 pos = sampleTriangleUniform(v0, v1, v2, r.yz);
    pos = (obj.transform * vec4(pos, 1.0)).xyz;
    norm = normalize(cross(v1 - v0, v2 - v0));
    norm = (obj.transformInvT * vec4(norm, 0.0)).xyz;
    dist = distance(ref, pos);

    return (pos - ref) / dist;
}

vec3 sampleLights(vec3 ref, out vec3 dir, out float dist, out float pdf, inout uint rng) {
    float sumPower = uLightSampleTable[0].prob;
    uint numLights = uLightSampleTable[0].failId;

    vec2 r = sample2f(rng);
    uint idx = uint(float(numLights) * r.x);
    idx = (r.y < uLightSampleTable[idx + 1].prob) ? idx : uLightSampleTable[idx + 1].failId - 1;

    vec3 radiance = uLightInstances[idx].radiance;
    ObjectInstance obj = uObjectInstances[uLightInstances[idx].objectIdx];

    vec3 norm;
    dir = sampleLight(obj, ref, dist, norm, sample3f(rng));

    float surfaceArea = obj.transformedSurfaceArea;
    float pdfSelectLight = luminance(radiance * surfaceArea) / sumPower;
    float pdfUniformPoint = 1.0 / surfaceArea;
    float cosTheta = dot(norm, dir);

    if (cosTheta <= 0.0) {
        pdf = 0.0;
        return Black;
    }
    else {
        pdf = luminance(radiance) / sumPower * dist * dist / absDot(norm, dir);
        return radiance;
    }
}

void main() {
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    ObjectInstance instance = uObjectInstances[gl_InstanceCustomIndexEXT];

    if (instance.lightIndex != InvalidResourceIdx) {
        rtPayload.radiance = uLightInstances[instance.lightIndex].radiance * instance.transformedSurfaceArea;
        return;
    }

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

	int texIdx = uMaterials[matIdx].textureIdx;

	if (texIdx == InvalidResourceIdx) {
	    albedo = uMaterials[matIdx].baseColor;
	}
	else {
	    albedo = texture(uTextures[texIdx], vec2(uvx, uvy)).rgb;
	}

    vec3 lightDir;
    float lightDist;
    float lightPdf;

    vec3 lightRadiance = sampleLights(pos, lightDir, lightDist, lightPdf, rtPayload.rng);

    if (lightPdf < 0) {
        return;
    }

    rtShadowPayload.hit = true;

    const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    
    traceRayEXT(
        /* accelerationStructureEXT TLAS */ uTLAS,
        /* uint rayFlags                 */ shadowRayFlags,
        /* uint cullMask                 */ 0xff,
        /* uint sbtRecordOffset          */ 0,
        /* uint sbtRecordStride          */ 0,
        /* uint missIndex                */ 1,
        /* vec3 rayOrigin                */ pos,
        /* float minDistance             */ MinRayDistance,
        /* vec3 rayDirection             */ lightDir,
        /* float maxDistance             */ lightDist - 1e-4,
        /* uint payloadLocation          */ 1
    );

    if (rtShadowPayload.hit) {
        return;
    }
    rtPayload.radiance += lightRadiance * albedo * PiInv * satDot(norm, lightDir) / lightPdf;
}
