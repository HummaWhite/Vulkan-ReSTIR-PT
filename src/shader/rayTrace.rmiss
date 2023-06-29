#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "rayTracingLayouts.glsl"

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main() {
    hitValue = vec3(0.0, 0.1, 0.3);
}