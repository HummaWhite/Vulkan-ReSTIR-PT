#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "ray_layouts.glsl"
#include "math.glsl"
#include "material.glsl"
#include "light_sampling.glsl"

layout(location = ClosestHitPayloadLocation) rayPayloadInEXT Intersection rtIsec;

hitAttributeEXT vec2 attribs;

void main() {
    rtIsec.bary = attribs;
    rtIsec.instanceIdx = gl_InstanceCustomIndexEXT;
    rtIsec.triangleIdx = gl_PrimitiveID;
}
