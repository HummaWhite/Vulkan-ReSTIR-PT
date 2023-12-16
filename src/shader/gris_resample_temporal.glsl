#ifndef GRIS_RESAMPLE_TEMPORAL_GLSL
#define GRIS_RESAMPLE_TEMPORAL_GLSL

#include "camera.glsl"
#include "gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "math.glsl"

vec3 temporalReuse(uvec2 index, uvec2 frameSize) {
    return vec3(1.0);
}

#endif