#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "rayTracingLayouts.glsl"
#include "math.glsl"

layout(location = 0) rayPayloadInEXT RTPayload prd;
hitAttributeEXT vec3 attribs;

void main() {
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    //vec3 bary = vec3(attribs.x, attribs.y, 1.0 - attribs.x - attribs.y);

    ObjectInstance instance = uObjectInstances[gl_InstanceCustomIndexEXT];

    uint i0 = uIndices[instance.indexOffset + gl_PrimitiveID * 3 + 0];
    uint i1 = uIndices[instance.indexOffset + gl_PrimitiveID * 3 + 1];
    uint i2 = uIndices[instance.indexOffset + gl_PrimitiveID * 3 + 2];

    MeshVertex v0 = uVertices[i0];
    MeshVertex v1 = uVertices[i1];
    MeshVertex v2 = uVertices[i2];

    //vec3 pos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 pos = v0.pos * bary.x + v1.pos * bary.y + v2.pos * bary.z;
    vec3 norm = v0.norm * bary.x + v1.norm * bary.y + v2.norm * bary.z;
    float uvx = v0.uvx * bary.x + v1.uvx * bary.y + v2.uvx * bary.z;
    float uvy = v0.uvy * bary.x + v1.uvy * bary.y + v2.uvy * bary.z;

    prd.radiance = vec3(uvx, uvy, 1.0);
    //prd.radiance = uVertices[7].norm;
    prd.radiance = norm;
    prd.radiance = colorWheel(float(i0) / 48);
    //prd.radiance = colorWheel(float(instance.indexOffset + gl_PrimitiveID * 3) / 72);
    //prd.radiance = vec3(instance.pad0, instance.pad1, instance.pad2);
}
