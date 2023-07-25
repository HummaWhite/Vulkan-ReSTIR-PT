#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "rayTracingLayouts.glsl"

layout(location = 0) rayPayloadInEXT RTPayload prd;
hitAttributeEXT vec3 attribs;

void main() {
    //prd.radiance = vec3(1.0, 0.0, 0.0);
    uint i0 = uIndices[gl_PrimitiveID * 3 + 0];
    uint i1 = uIndices[gl_PrimitiveID * 3 + 1];
    uint i2 = uIndices[gl_PrimitiveID * 3 + 2];

    MeshVertex v0 = uVertices[i0];
    MeshVertex v1 = uVertices[i1];
    MeshVertex v2 = uVertices[i2];

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    prd.radiance = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
}
