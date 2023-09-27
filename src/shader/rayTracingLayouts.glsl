#ifndef RAYTRACING_LAYOUTS_GLSL
#define RAYTRACING_LAYOUTS_GLSL

#include "layouts.glsl"

const float MinRayDistance = 1e-4;
const float MaxRayDistance = 1e7;

struct RTPayload {
	vec3 radiance;
	vec3 throughput;
	uint curDepth;
	uint rng;
};

struct RTShadowPayload {
	bool hit;
};

layout(set = RayTracingDescSet, binding = 0) uniform accelerationStructureEXT uTLAS;

#endif