#ifndef LAYOUTS_GLSL
#define LAYOUTS_GLSL

#include "HostDevice.h"

struct Material {
	vec3 baseColor;
	uint type;

	uint textureIdx;
	float metallic;
	float roughness;
	float ior;
#if !RESTIR_PT_MATERIAL
	float specular;
	float specularTint;
	float sheen;
	float sheenTint;

	float clearcoat;
	float clearcoatGloss;
	float subsurface;
	int pad0;
#endif
};

struct Camera {
	mat4 view;
	mat4 proj;
	mat4 projView;
	mat4 lastProjView;

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

	vec3 radiance;
	float pad0;

	uint indexOffset;
	uint indexCount;
	uint matIndex;
	float pad2;
};

struct TriangleLight {
	vec3 v0;
	float nx;
	vec3 v1;
	float ny;
	vec3 v2;
	float nz;
	vec3 radiance;
	float area;
};

struct LightSampleTableElement {
	float prob;
	uint failId;
};

struct Intersection {
	vec2 bary;
	uint instanceIdx;
	uint triangleIdx;
};

struct DIPathSample {
	vec3 Li;
	float pad0;
	vec3 rcPos;
	bool valid;
};

struct DIReservoir {
	DIPathSample pathSample;

	uint sampleCount;
	float resampleWeight;
	float contribWeight;
	float pHat;
};

struct GIPathSample {
	Intersection rcIsec;
	vec3 rcLo;
	uint rcPrevCoord;
};

struct GIReservoir {
	GIPathSample pathSample;

	uint sampleCount;
	float resampleWeight;
	float contribWeight;
	float pad0;
};

struct GRISPathSample {
	Intersection rcIsec;

	vec3 rcLi;
	bool rcIsLight;

	vec3 rcWi;
	uint flags;

	vec3 rcLs;
	float rcPrevScatterPdf;

	vec3 rcWs;
	float rcJacobian;

	uint rcRng;
	uint primaryRng;
	uint pad0;
	uint pad1;
};

struct GRISReservoir {
	GRISPathSample pathSample;

	float sampleCount;
	float resampleWeight;
	float contribWeight;
	float pad0;
};

struct GRISReconnectionData {
	Intersection rcPrevIsec;
	vec3 rcPrevWo;
	float pad0;
	vec3 rcPrevThroughput;
	float pad1;
};

layout(set = CameraDescSet, binding = 0) uniform _Camera {
	Camera uCamera;
	Camera uPrevCamera;
};

layout(set = ResourceDescSet, binding = 0) uniform sampler2D uTextures[];
layout(set = ResourceDescSet, binding = 1) readonly buffer _Materials { Material uMaterials[]; };
layout(set = ResourceDescSet, binding = 2) readonly buffer _MaterialIndices { int uMaterialIndices[]; };
layout(set = ResourceDescSet, binding = 3) readonly buffer _Vertices { MeshVertex uVertices[]; };
layout(set = ResourceDescSet, binding = 4) readonly buffer _Indices { uint uIndices[]; };
layout(set = ResourceDescSet, binding = 5) readonly buffer _ObjectInstances { ObjectInstance uObjectInstances[]; };
layout(set = ResourceDescSet, binding = 6) readonly buffer _TriangleLights { TriangleLight uTriangleLights[]; };
layout(set = ResourceDescSet, binding = 7) readonly buffer _LightSampleTable { LightSampleTableElement uLightSampleTable[]; };

layout(set = RayImageDescSet, binding =  0, rgba16f) uniform image2D uDirectOutput;
layout(set = RayImageDescSet, binding =  1, rgba16f) uniform image2D uIndirectOutput;
layout(set = RayImageDescSet, binding =  2) uniform sampler2D uDepthNormal;
layout(set = RayImageDescSet, binding =  3) uniform sampler2D uDepthNormalPrev;
layout(set = RayImageDescSet, binding =  4) uniform usampler2D uAlbedoMatId;
layout(set = RayImageDescSet, binding =  5) uniform usampler2D uAlbedoMatIdPrev;
layout(set = RayImageDescSet, binding =  6) uniform sampler2D uMotionVector;
layout(set = RayImageDescSet, binding =  7) buffer _DIReservoirThis { DIReservoir uDIReservoir[]; };
layout(set = RayImageDescSet, binding =  8) buffer _DIReservoirPrev { DIReservoir uDIReservoirPrev[]; };
layout(set = RayImageDescSet, binding =  9) buffer _GIReservoirThis { GIReservoir uGIReservoir[]; };
layout(set = RayImageDescSet, binding = 10) buffer _GIReservoirPrev { GIReservoir uGIReservoirPrev[]; };
layout(set = RayImageDescSet, binding = 11) buffer _GRISReservoirThis { GRISReservoir uGRISReservoir[]; };
layout(set = RayImageDescSet, binding = 12) buffer _GRISReservoirPrev { GRISReservoir uGRISReservoirPrev[]; };
layout(set = RayImageDescSet, binding = 13) buffer _GRISReconnectionData { GRISReconnectionData uGRISReconnectionData[]; };

#endif