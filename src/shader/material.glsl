#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "math.glsl"

const uint Diffuse = 1 << 0;
const uint Glossy = 1 << 1;
const uint Specular = 1 << 2;
const uint Reflection = 1 << 4;
const uint Transmission = 1 << 5;

const uint Lambert = 0;
const uint Principled = 1;
const uint Metalworkflow = 2;
const uint Dielectric = 3;
const uint ThinDielectric = 4;
const uint Light = 5;

/*
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
*/

struct BSDFSample {
	vec3 wi;
	float pdf;
	vec3 bsdf;
	uint type;
};

vec3 lambertBSDF(vec3 albedo, vec3 n, vec3 wi) {
	return albedo * PiInv;
}

float lambertPDF(vec3 n, vec3 wi) {
	return absDot(n, wi) * PiInv;
}

bool lambertSampleBSDF(vec3 albedo, vec3 n, vec2 r, out BSDFSample s) {
	s.wi = sampleCosineWeightedHemisphere(n, r);
	s.pdf = absDot(n, s.wi) * PiInv;
	s.bsdf = albedo * PiInv;
	s.type = Diffuse | Reflection;
	return true;
}

vec3 BSDF(Material mat, vec3 albedo, vec3 n, vec3 wo, vec3 wi) {
	return lambertBSDF(albedo, n, wi);
}

float PDF(Material mat, vec3 n, vec3 wo, vec3 wi) {
	return lambertPDF(n, wi);
}

bool sampleBSDF(Material mat, vec3 albedo, vec3 n, vec3 wo, vec3 r, out BSDFSample s) {
	return lambertSampleBSDF(albedo, n, r.xy, s);
}

#endif