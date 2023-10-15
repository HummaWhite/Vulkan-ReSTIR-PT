#ifndef RESERVOIR_GLSL
#define RESERVOIR_GLSL

#include "material.glsl"
#include "math.glsl"

GIPathSample GIPathSampleInit() {
	GIPathSample pathSample;
	pathSample.radiance = vec3(0.0);
	pathSample.sampledNorm = vec3(100.0);
	return pathSample;
}

bool GIPathSampleIsValid(GIPathSample pathSample) {
	return pathSample.sampledNorm.x < 10.0;
}

void GIReservoirReset(inout GIReservoir resv) {
	resv.sampleCount = 0;
	resv.resampleWeight = 0.0;
	resv.contribWeight = 0.0;
}

bool GIReservoirIsValid(GIReservoir resv) {
	return !isnan(resv.resampleWeight) && resv.resampleWeight >= 0;
}

void GIReservoirResetIfInvalid(inout GIReservoir resv) {
	if (!GIReservoirIsValid(resv)) {
		GIReservoirReset(resv);
	}
}

void GIReservoirUpdateContrib(inout GIReservoir resv, float pHat) {
	resv.contribWeight = resv.resampleWeight / (float(resv.sampleCount) * pHat);
}

void GIReservoirAddSample(inout GIReservoir resv, GIPathSample pathSample, float resampleWeight, float r) {
	resv.resampleWeight += resampleWeight;
	resv.sampleCount++;

	if (r * resv.resampleWeight < resampleWeight) {
		resv.pathSample = pathSample;
	}
}

void GIReservoirMerge(inout GIReservoir resv, GIReservoir rhs, float pHat, float rand) {
	uint sampleCount = resv.sampleCount;
	GIReservoirAddSample(resv, rhs.pathSample, rhs.contribWeight * pHat * float(rhs.sampleCount), rand);
	resv.sampleCount += rhs.sampleCount;
}

void GIReservoirCapSample(inout GIReservoir resv, uint cap) {
	if (resv.sampleCount > cap) {
		resv.resampleWeight *= float(cap) / float(resv.sampleCount);
		resv.sampleCount = cap;
	}
}

float GIPathSamplePHat(GIPathSample pathSample) {
	return luminance(pathSample.radiance);
}

#endif