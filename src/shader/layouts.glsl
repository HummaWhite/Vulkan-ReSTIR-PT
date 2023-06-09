#ifndef LAYOUTS_GLSL
#define LAYOUTS_GLSL

#include "HostDevice.h"

struct Material {
	vec3 baseColor;
	uint type;

	int textureIdx;
	float metallic;
	float roughness;
	float ior;

	float specular;
	float specularTint;
	float sheen;
	float sheenTint;

	float clearcoat;
	float clearcoatGloss;
	float subsurface;
	int pad0;
};

struct Camera{
	mat4 view;
	mat4 proj;
	mat4 projView;
	mat4 lastProjView;
	vec3 pos;
	vec3 angle;
	vec3 front;
	vec3 right;
	vec3 up;
	ivec2 filmSize;
	float FOV;
	float near;
	float far;
	float lensRadius;
	float focalDist;
};

struct GBufferDrawParam {
	mat4 model;
	mat4 modelInvT;
	int matIdx;
	int pad0;
	int pad1;
	int pad2;
};

struct Reservoir {
	uint pad;
};

struct Ray {
	vec3 ori;
	vec3 dir;
};


layout(push_constant) uniform _PushConstant {
	GBufferDrawParam uGBufferDrawParam;
};


layout(set = CameraDescSet, binding = 0, std140) uniform _Camera {
	Camera uCamera;
};


layout(set = ResourceDescSet, binding = 0) uniform sampler2D uTextures[];

layout(set = ResourceDescSet, binding = 1, std140) readonly buffer _Materials {
	Material uMaterials[];
};

layout(set = ResourceDescSet, binding = 2, std140) readonly buffer _MaterialIndices {
	int uMaterialIndices[];
};


layout(set = GBufferDrawParamDescSet, binding = 0, std140) readonly buffer _GBufferDrawParam {
	GBufferDrawParam uGBufferDrawParams[];
};


layout(set = ImageOutputDescSet, binding = 0) uniform usampler2D uGBufferA;
layout(set = ImageOutputDescSet, binding = 1) uniform usampler2D uGBufferB;
layout(set = ImageOutputDescSet, binding = 2, rgba32f) uniform image2D uRayColorOutput;

layout(set = ImageOutputDescSet, binding = 3) buffer _Reservoir {
	Reservoir uReservoirs[];
};


// in ratTraceLayouts.glsl
// layout(set = RayTracingDescSet, binding = 0) uniform accelerationStructureEXT uTLAS;

#endif