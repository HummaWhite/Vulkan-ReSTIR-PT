#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "rayTracingLayouts.glsl"

layout(location = 0) rayPayloadInEXT RTPayload rtPayload;

void main() {
    rtPayload.radiance = vec3(0.4, 0.6, 1.0);
}