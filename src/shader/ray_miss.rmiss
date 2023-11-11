#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "ray_layouts.glsl"

layout(location = ClosestHitPayloadLocation) rayPayloadInEXT Intersection rtIsec;

void main() {
    rtIsec.hit = false;
}