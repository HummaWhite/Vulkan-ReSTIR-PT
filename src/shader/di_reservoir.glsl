#ifndef DI_RESERVOIR_GLSL
#define DI_RESERVOIR_GLSL

#include "material.glsl"
#include "math.glsl"
#include "ray_layouts.glsl"

const uint SampleModeLight = 0;
const uint SampleModeBSDF = 1;
const uint SampleModeBoth = 2;

void DIPathSampleInit(inout DIPathSample pathSample) {
	pathSample.isec.instanceIdx = InvalidHitIndex;
}

bool DIPathSampleIsValid(DIPathSample pathSample) {
	return pathSample.isec.instanceIdx != InvalidHitIndex;
}

float DIToScalar(vec3 color) {
	return luminance(color);
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

void DIReservoirAddSample(inout DIReservoir resv, DIPathSample pathSample, float resampleWeight, float r) {
	resv.resampleWeight += resampleWeight;
	resv.sampleCount++;

	if (r * resv.resampleWeight < resampleWeight) {
		resv.pathSample = pathSample;
		resv.weight = resampleWeight;
	}
}

void DIReservoirMerge(inout DIReservoir resv, DIReservoir rhs, float r) {
	resv.resampleWeight += rhs.resampleWeight;
	resv.sampleCount += rhs.sampleCount;
	
	if (r * resv.resampleWeight < rhs.resampleWeight) {
		resv.pathSample = rhs.pathSample;
	}
}

void DIReservoirCapSample(inout DIReservoir resv, uint cap) {
	if (resv.sampleCount > cap) {
		resv.resampleWeight *= float(cap) / float(resv.sampleCount);
		resv.sampleCount = cap;
	}
}

#endif