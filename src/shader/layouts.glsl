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

struct Camera {
	mat4 view;
	mat4 viewInv;
	mat4 proj;
	mat4 projInv;
	mat4 projView;
	mat4 projViewInv;
	mat4 lastProjView;
	mat4 lastProjViewInv;

	vec3 pos;
	float FOV;

	vec3 angle;
	float near;

	vec3 front;
	float far;

	vec3 right;
	float lensRadius;

	vec3 up;
	float focalDist;

	uvec2 filmSize;
	uint frameIndex;
	uint seed;
};

struct GBufferDrawParam {
	mat4 model;
	mat4 modelInvT;
	int matIdx;
	int pad0;
	int pad1;
	int pad2;
};

struct Ray {
	vec3 ori;
	vec3 dir;
};

struct MeshVertex {
	vec3 pos;
	float uvx;
	vec3 norm;
	float uvy;
};

struct ObjectInstance {
	mat4 transform;
	mat4 transformInv;
	mat4 transformInvT;
	uint indexOffset;
	uint indexCount;
	float transformedSurfaceArea;
	int lightIndex;
};

struct LightInstance {
	vec3 radiance;
	uint objectIdx;
};

struct LightSampleTableElement {
	float prob;
	uint failId;
};

struct DIPathSample {
	vec3 radiance;
	float pHat;
	vec3 wi;
	float dist;
};

struct GIPathSample {
	vec3 radiance;
	vec3 visiblePos;
	vec3 visibleNorm;
	vec3 sampledPos;
	vec3 sampledNorm;
};

struct DIReservoir {
	DIPathSample pathSample;
	uint sampleCount;
	float resampleWeight;
	float pad0;
	float pad1;
};

struct GIReservoir {
	vec3 radiance;
	uint sampleCount;
	vec3 visiblePos;
	float resampleWeight;
	vec3 visibleNorm;
	float contribWeight;
	vec3 sampledPos;
	float pHat;
};

struct PTReservoir {
	uint pad;
};

layout(set = CameraDescSet, binding = 0) uniform _Camera {
	Camera uCamera;
	Camera uPrevCamera;
};

layout(set = GBufferDrawParamDescSet, binding = 0, std140) readonly buffer _GBufferDrawParam { GBufferDrawParam uGBufferDrawParams[]; };

layout(set = ResourceDescSet, binding = 0) uniform sampler2D uTextures[];
layout(set = ResourceDescSet, binding = 1) readonly buffer _Materials { Material uMaterials[]; };
layout(set = ResourceDescSet, binding = 2) readonly buffer _MaterialIndices { int uMaterialIndices[]; };
layout(set = ResourceDescSet, binding = 3) readonly buffer _Vertices { MeshVertex uVertices[]; };
layout(set = ResourceDescSet, binding = 4) readonly buffer _Indices { uint uIndices[]; };
layout(set = ResourceDescSet, binding = 5) readonly buffer _ObjectInstances { ObjectInstance uObjectInstances[]; };
layout(set = ResourceDescSet, binding = 6) readonly buffer _LightInstances { LightInstance uLightInstances[]; };
layout(set = ResourceDescSet, binding = 7) readonly buffer _LightSampleTable { LightSampleTableElement uLightSampleTable[]; };

layout(set = RayImageDescSet, binding = 0, rgba32f) uniform image2D uDirectOutput;
layout(set = RayImageDescSet, binding = 1, rgba32f) uniform image2D uIndirectOutput;
layout(set = RayImageDescSet, binding = 2) uniform usampler2D uGBufferThisA;
layout(set = RayImageDescSet, binding = 3) uniform usampler2D uGBufferPrevA;
layout(set = RayImageDescSet, binding = 4) uniform usampler2D uGBufferThisB;
layout(set = RayImageDescSet, binding = 5) uniform usampler2D uGBufferPrevB;
layout(set = RayImageDescSet, binding = 6) buffer _DIReservoirThis { DIReservoir uDIReservoirThis[]; };
layout(set = RayImageDescSet, binding = 7) buffer _DIReservoirPrev { DIReservoir uDIReservoirPrev[]; };
layout(set = RayImageDescSet, binding = 8) buffer _GIReservoirThis { GIReservoir uGIReservoirThis[]; };
layout(set = RayImageDescSet, binding = 9) buffer _GIReservoirPrev { GIReservoir uGIReservoirPrev[]; };

// in ratTraceLayouts.glsl
// layout(set = RayTracingDescSet, binding = 0) uniform accelerationStructureEXT uTLAS;

#endif