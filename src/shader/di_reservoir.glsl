#ifndef DI_RESERVOIR_GLSL
#define DI_RESERVOIR_GLSL

#include "material.glsl"
#include "math.glsl"

DIPathSample DIPathSampleInit() {
	DIPathSample pathSample;
	pathSample.radiance = vec3(0.0);
	pathSample.dist = -1.0;
	return pathSample;
}

bool DIPathSampleIsValid(DIPathSample pathSample) {
	return pathSample.dist > 0;
}

void DIReservoirReset(inout DIReservoir resv) {
	resv.sampleCount = 0;
	resv.resampleWeight = 0.0;
	resv.contribWeight = 0.0;
}

bool DIReservoirIsValid(DIReservoir resv) {
	return !isnan(resv.resampleWeight) && resv.resampleWeight >= 0;
}

void DIReservoirResetIfInvalid(inout DIReservoir resv) {
	if (!DIReservoirIsValid(resv)) {
		DIReservoirReset(resv);
	}
}

void DIReservoirUpdateContrib(inout DIReservoir resv, float pHat) {
	resv.contribWeight = resv.resampleWeight / (float(resv.sampleCount) * pHat);
}

void DIReservoirAddSample(inout DIReservoir resv, DIPathSample pathSample, float resampleWeight, float r) {
	resv.resampleWeight += resampleWeight;
	resv.sampleCount++;

	if (r * resv.resampleWeight < resampleWeight) {
		resv.radiance = pathSample.radiance;
		resv.wi = pathSample.wi;
		resv.dist = pathSample.dist;
	}
}

void DIReservoirMerge(inout DIReservoir resv, DIReservoir rhs, float r) {
	resv.resampleWeight += rhs.resampleWeight;
	resv.sampleCount += rhs.sampleCount;
	
	if (r * resv.resampleWeight < rhs.resampleWeight) {
		resv.radiance = rhs.radiance;
		resv.wi = rhs.wi;
		resv.dist = rhs.dist;
	}
}

void DIReservoirCapSample(inout DIReservoir resv, uint cap) {
	if (resv.sampleCount > cap) {
		resv.resampleWeight *= float(cap) / float(resv.sampleCount);
		resv.sampleCount = cap;
	}
}

float DIPathSamplePHat(DIPathSample pathSample) {
	return luminance(pathSample.radiance);
}

#endif