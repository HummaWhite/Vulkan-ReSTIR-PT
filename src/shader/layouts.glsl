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

layout(push_constant) uniform _PushConstant{
	GBufferDrawParam uGBufferDrawParam;
};

layout(set = CameraDescSet, binding = 0, std140) uniform _Camera{
	Camera uCamera;
};

layout(set = ResourceDescSet, binding = 0) uniform sampler2D uTexture;

layout(set = ResourceDescSet, binding = 1, std140) readonly buffer _Materials {
	Material uMaterials[];
};

layout(set = ResourceDescSet, binding = 2, std140) readonly buffer _MaterialIndices {
	int uMaterialIndices[];
};

layout(set = GBufferDrawParamDescSet, binding = 0, std140) readonly buffer _GBufferDrawParam {
	GBufferDrawParam uGBufferDrawParams[];
};

layout(set = SwapchainStorageDescSet, binding = 0) uniform sampler2D uDepthNormal;
layout(set = SwapchainStorageDescSet, binding = 1) uniform sampler2D uAlbedoMatIdx;

#endif