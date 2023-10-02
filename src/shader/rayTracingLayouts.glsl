#ifndef RAYTRACING_LAYOUTS_GLSL
#define RAYTRACING_LAYOUTS_GLSL

#include "layouts.glsl"

const float MinRayDistance = 1e-4;
const float MaxRayDistance = 1e7;

struct CompactMaterial {
	uint data;
	uint type;
};

// Don't make it larger than 32 registers
struct Intersection {
	vec3 pos;
	uint instanceIndex;

	vec3 norm;
	int lightIndex;

	vec3 albedo;
	float transSurfaceArea;

	int matIndex;
	bool hit;
};

struct RTShadowPayload {
	bool hit;
};

layout(set = RayTracingDescSet, binding = 0) uniform accelerationStructureEXT uTLAS;

#endif