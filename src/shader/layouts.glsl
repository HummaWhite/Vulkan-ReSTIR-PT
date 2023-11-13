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

struct GBufferDrawParam {
	mat4 model;
	mat4 modelInvT;
	int matIdx;
	int meshIdx;
	int pad0;
	int pad1;
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
	float pad1;
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

struct DIPathSample {
	vec3 radiance;
	float pHat;
	vec3 wi;
	float dist;
};

struct DIReservoir {
	vec3 radiance;
	float pad0;
	vec3 wi;
	float dist;
	uint sampleCount;
	float resampleWeight;
	float contribWeight;
	float pHat;
};

struct GIPathSample {
	vec3 radiance;
	vec3 visiblePos;
	vec3 visibleNorm;
	vec3 sampledPos;
	vec3 sampledNorm;
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

struct GRISPathSample {
	vec2 rcVertexBary;
	uint rcVertexInstanceIdx;
	uint rcVertexTriangleIdx;
	vec3 rcVertexIrradiance;
	float rcVertexLightPdf;
	vec3 rcVertexWi;
	uint rcVertexRng;
	vec3 cachedJacobian;
	uint primaryHitRng;
	vec3 F;
	uint pathFlags;
};

struct GRISReservoir {
	GRISPathSample pathSample;
	float sampleCount;
	float resampleWeight;
	float contribWeight;
	float pad0;
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
layout(set = ResourceDescSet, binding = 6) readonly buffer _TriangleLights { TriangleLight uTriangleLights[]; };
layout(set = ResourceDescSet, binding = 7) readonly buffer _LightSampleTable { LightSampleTableElement uLightSampleTable[]; };

layout(set = RayImageDescSet, binding = 0, rgba16f) uniform image2D uDirectOutput;
layout(set = RayImageDescSet, binding = 1, rgba16f) uniform image2D uIndirectOutput;
layout(set = RayImageDescSet, binding = 2) uniform sampler2D uDepthNormal;
layout(set = RayImageDescSet, binding = 3) uniform sampler2D uDepthNormalPrev;
layout(set = RayImageDescSet, binding = 4) uniform usampler2D uAlbedoMatId;
layout(set = RayImageDescSet, binding = 5) uniform usampler2D uAlbedoMatIdPrev;
layout(set = RayImageDescSet, binding = 6) buffer _DIReservoirThis { DIReservoir uDIReservoir[]; };
layout(set = RayImageDescSet, binding = 7) buffer _DIReservoirPrev { DIReservoir uDIReservoirPrev[]; };
layout(set = RayImageDescSet, binding = 8) buffer _GIReservoirThis { GIReservoir uGIReservoir[]; };
layout(set = RayImageDescSet, binding = 9) buffer _GIReservoirPrev { GIReservoir uGIReservoirPrev[]; };
layout(set = RayImageDescSet, binding = 10) uniform sampler2D uMotionVector;

#endif