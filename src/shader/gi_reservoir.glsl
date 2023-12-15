#ifndef GI_RESERVOIR_GLSL
#define GI_RESERVOIR_GLSL

#include "material.glsl"
#include "math.glsl"
#include "ray_layouts.glsl"

GIPathSample GIPathSampleInit() {
	GIPathSample pathSample;
	pathSample.rcIsec.instanceIdx = InvalidHitIndex;
	return pathSample;
}

bool GIPathSampleIsValid(GIPathSample pathSample) {
	return pathSample.rcIsec.instanceIdx != InvalidHitIndex;
}

float GIToScalar(vec3 color) {
	return luminance(color);
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

void GIReservoirAddSample(inout GIReservoir resv, GIPathSample pathSample, float resampleWeight, float r) {
	resv.resampleWeight += resampleWeight;
	resv.sampleCount++;

	if (r * resv.resampleWeight < resampleWeight) {
		resv.pathSample = pathSample;
	}
}

void GIReservoirCapSample(inout GIReservoir resv, uint cap) {
	if (resv.sampleCount > cap) {
		resv.resampleWeight *= float(cap) / float(resv.sampleCount);
		resv.sampleCount = cap;
	}
}

#endif