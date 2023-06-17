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

layout(set = SceneResourceDescSet, binding = 0, std140) uniform _CameraData{
	mat4 model;
	mat4 view;
	mat4 proj;
} uCamera;

layout(set = SceneResourceDescSet, binding = 1) uniform sampler2D uTexture;

layout(set = SceneResourceDescSet, binding = 2, std140) readonly buffer _Materials {
	Material uMaterials[];
};

layout(set = SceneResourceDescSet, binding = 3, std140) readonly buffer _MaterialIndices {
	uint uMaterialIndices[];
};

layout(set = SwapchainStorageDescSet, binding = 0) uniform sampler2D uDepthNormal;
layout(set = SwapchainStorageDescSet, binding = 1) uniform sampler2D uAlbedoMatIdx;

#endif