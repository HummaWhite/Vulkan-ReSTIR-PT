#ifndef GRIS_RESERVOIR_GLSL
#define GRIS_RESERVOIR_GLSL

#include "material.glsl"
#include "math.glsl"
#include "ray_layouts.glsl"

const uint GRISMergeModeRegular = 0;
const uint GRISMergeModeWithMISResample = 1;
const uint GRISMaxPathLength = 15;
const float GRISDistanceThreshold = 0.01;

const uint Reconnection = 0;
const uint Replay = 1;
const uint Hybrid = 2;

float GRISToScalar(vec3 color) {
	return luminance(color);
}

uint GRISPathFlagsRcVertexId(uint flags) {
	return flags & 0x1f;
}

void GRISPathFlagsSetRcVertexId(inout uint flags, uint id) {
	flags = (flags & 0xffffffe0) | (id & 0x1f);
}

uint GRISPathFlagsPathLength(uint flags) {
	return (flags >> 5) & 0x1f;
}

void GRISPathFlagsSetPathLength(inout uint flags, uint id) {
	flags = (flags & 0xfffffc1f) | ((id & 0x1f) << 5);
}

void GRISPathSampleReset(inout GRISPathSample pathSample) {
	pathSample.rcIsec.instanceIdx = InvalidHitIndex;
	pathSample.rcLi = vec3(0.0);
	pathSample.rcWi = vec3(0.0);
	pathSample.rcLs = vec3(0.0);
	pathSample.rcWs = vec3(0.0);
	pathSample.rcPrevScatterPdf = 0;
	pathSample.rcJacobian = 0;
	pathSample.rcIsLight = false;
	pathSample.flags = 0;
}

bool GRISPathSampleIsValid(GRISPathSample pathSample) {
	return pathSample.rcIsec.instanceIdx != InvalidHitIndex;
}

void GRISReservoirReset(inout GRISReservoir resv) {
	//GRISPathSampleReset(resv.pathSample);
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

bool GRISReservoirAddSample(inout GRISReservoir resv, GRISPathSample pathSample, float weight, float r) {
	resv.sampleCount += 1.0;
	resv.resampleWeight += weight;

	if (r * resv.resampleWeight < weight) {
		resv.pathSample = pathSample;
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

/*
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

	if (sample1f(rng) * resv.resampleWeight < resampleWeight) {
		resv.pathSample = rhs.pathSample;
		return true;
	}
	return false;
}

bool GRISReservoirMerge(inout GRISReservoir resv, GRISReservoir rhs, vec3 F, float jacobian, inout uint rng, float MISWeight, bool forceAdd) {
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

bool GRISReservoirMerge(inout GRISReservoir resv, GRISReservoir rhs, vec3 F, float jacobian, inout uint rng) {
	return GRISReservoirMerge(resv, rhs, F, jacobian, rng, 1.0, false);
}

bool GRISReservoirMergeWithMISResample(inout GRISReservoir resv, GRISReservoir rhs, vec3 F, float jacobian, inout uint rng, float MISWeight, bool forceAdd) {
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

bool GRISReservoirMergeWithMISResample(inout GRISReservoir resv, GRISReservoir rhs, vec3 F, float jacobian, inout uint rng) {
	return GRISReservoirMergeWithMISResample(resv, rhs, F, jacobian, rng, 1.0, false);
}
*/

#endif