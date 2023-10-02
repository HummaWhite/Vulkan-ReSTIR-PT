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

layout(set = RayTracingDescSet, binding = 0) uniform accelerationStructureEXT uTLAS;

#endif