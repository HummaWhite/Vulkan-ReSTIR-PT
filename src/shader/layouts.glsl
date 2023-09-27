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

	ivec2 filmSize;
	uint seed0;
	uint seed1;
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


layout(set = CameraDescSet, binding = 0) uniform _Camera {
	Camera uCamera;
};

layout(set = ResourceDescSet, binding = 0) uniform sampler2D uTextures[];

layout(set = ResourceDescSet, binding = 1) readonly buffer _Materials {
	Material uMaterials[];
};

layout(set = ResourceDescSet, binding = 2) readonly buffer _MaterialIndices {
	int uMaterialIndices[];
};

layout(set = ResourceDescSet, binding = 3) readonly buffer _Vertices {
	MeshVertex uVertices[];
};

layout(set = ResourceDescSet, binding = 4) readonly buffer _Indices {
	uint uIndices[];
};

layout(set = ResourceDescSet, binding = 5) readonly buffer _ObjectInstances {
	ObjectInstance uObjectInstances[];
};

layout(set = ResourceDescSet, binding = 6) readonly buffer _LightInstances {
	LightInstance uLightInstances[];
};

layout(set = ResourceDescSet, binding = 7) readonly buffer _LightSampleTable {
	LightSampleTableElement uLightSampleTable[];
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