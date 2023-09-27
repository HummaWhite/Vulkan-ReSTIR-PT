#ifndef RAYTRACING_LAYOUTS_GLSL
#define RAYTRACING_LAYOUTS_GLSL

#include "layouts.glsl"

const float MinRayDistance = 1e-4;
const float MaxRayDistance = 1e7;

struct Intersection {
	vec3 pos;
	vec3 norm;
	vec3 albedo;
	vec3 bary;
	vec2 uv;
	uint instanceIndex;
	int lightIndex;
	float transSurfaceArea;
	int matIndex;
	int texIndex;
	bool hit;
};

struct RTShadowPayload {
	bool hit;
};

layout(set = RayTracingDescSet, binding = 0) uniform accelerationStructureEXT uTLAS;

#endif