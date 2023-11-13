#ifndef GRIS_RESERVOIR_GLSL
#define GRIS_RESERVOIR_GLSL

#include "material.glsl"
#include "math.glsl"
#include "ray_layouts.glsl"

const uint GRISMergeModeRegular = 0;
const uint GRISMergeModeWithMISResample = 1;

float GRISToScalar(vec3 color) {
	return luminance(color);
}

void GRISPathSampleReset(inout GRISPathSample pathSample) {
	pathSample.rcVertexInstanceIdx = InvalidHitIndex;
}

void GRISReservoirReset(inout GRISReservoir resv) {
	GRISPathSampleReset(resv.pathSample);
	resv.F = vec3(0.0);
	resv.pathFlags = 0;
	resv.sampleCount = 0;
	resv.resampleWeight = 0;
}

bool isResampleWeightInvalid(float weight) {
	return isnan(weight) || weight == 0;
}

bool GRISReservoirIsValid(GRISReservoir resv) {
	return !isnan(resv.resampleWeight) && resv.resampleWeight >= 0;
}

void GRISReservoirResetIfInvalid(inout GRISReservoir resv) {
	if (!GRISReservoirIsValid(resv)) {
		GRISReservoirReset(resv);
	}
}

void GRISReservoirUpdateContribWeight(inout GRISReservoir resv) {
	resv.contribWeight = resv.resampleWeight / (resv.sampleCount * GRISToScalar(resv.pathSample.F));
}

bool GRISReservoirAdd(inout GRISReservoir resv, vec3 F, float pHat, inout uint rng) {
	resv.sampleCount += 1.0;
	float resampleWeight = GRISToScalar(F) / pHat;

	if (isResampleWeightInvalid(resampleWeight)) {
		return false;
	}
	resv.resampleWeight += resampleWeight;

	if (sample1f(rng) * resv.resampleWeight < resampleWeight) {
		resv.pathSample.F = F;
		return true;
	}
	return false;
}

bool GRISReservoirMerge(inout GRISReservoir resv, GRISReservoir rhs, inout uint rng) {
	resv.sampleCount += rhs.sampleCount;
	float resampleWeight = rhs.resampleWeight;

	if (isResampleWeightInvalid(resampleWeight)) {
		return false;
	}
	resv.resampleWeight += resampleWeight;

	if (sample1f(rng) * resv.resampleWeight < resampleWeight || forceAdd) {
		resv.pathSample = rhs.pathSample;
		return true;
	}
	return false;
}

bool GRISReservoirMerge(inout GRISReservoir resv, GRISReservoir rhs, vec3 F, float jacobian, inout uint rng, float MISWeight = 1.0, bool forceAdd = false) {
	resv.sampleCount += rhs.sampleCount;
	float resampleWeight = GRISToScalar(F) * jacobian * rhs.sampleCount * rhs.resampleWeight;

	if (isResampleWeightInvalid(resampleWeight)) {
		return false;
	}
	resv.resampleWeight += resampleWeight;

	if (sample1f(rng) * resv.resampleWeight < resampleWeight || forceAdd) {
		resv.pathSample = rhs.pathSample;
		resv.pathSample.F = F;
		return true;
	}
	return false;
}

bool GRISReservoirMergeWithMISResample(inout GRISReservoir resv, GRISReservoir rhs, vec3 F, float jacobian, inout uint rng, float MISWeight = 1.0, bool forceAdd = false) {
	resv.sampleCount += rhs.sampleCount;
	float resampleWeight = GRISToScalar(F) * jacobian * rhs.resampleWeight;

	if (isResampleWeightInvalid(resampleWeight)) {
		return false;
	}
	resv.resampleWeight += resampleWeight;

	if (sample1f(rng) * resv.resampleWeight < resampleWeight || forceAdd) {
		resv.pathSample = rhs.pathSample;
		resv.pathSample.F = F;
		return true;
	}
	return false;
}

void GRISReservoirCapSample(inout GRISReservoir resv, float cap) {
	if (resv.sampleCount > cap) {
		resv.resampleWeight *= cap / resv.sampleCount;
		resv.sampleCount = cap;
	}
}

#endif