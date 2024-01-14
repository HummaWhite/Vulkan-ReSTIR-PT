#ifndef DI_RESERVOIR_GLSL
#define DI_RESERVOIR_GLSL

#include "material.glsl"
#include "math.glsl"
#include "ray_layouts.glsl"

const uint SampleModeLight = 0;
const uint SampleModeBSDF = 1;
const uint SampleModeBoth = 2;

const uint Reconnection = 0;
const uint Replay = 1;
const uint Hybrid = 2;

struct Settings {
	uint shiftMode;
	uint sampleMode;
	bool temporalReuse;
	bool spatialReuse;
};

layout(push_constant) uniform _Settings{
	Settings uSettings;
};

void DIPathSampleInit(inout DIPathSample pathSample) {
	pathSample.isec.instanceIdx = InvalidHitIndex;
}

bool DIPathSampleIsValid(DIPathSample pathSample) {
	return pathSample.isec.instanceIdx != InvalidHitIndex;
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
        resv.weight = rhs.weight;
    }
}

void DIReservoirCapSample(inout DIReservoir resv, uint cap) {
	if (resv.sampleCount > cap) {
		resv.resampleWeight *= float(cap) / float(resv.sampleCount);
		resv.sampleCount = cap;
	}
}

vec3 sampleLi(SurfaceInfo surf, Material mat, vec3 wo, uint rng, inout uint resvRng, inout DIReservoir resv) {
    vec3 radiance = vec3(0.0);
    DIPathSample pathSample;
    pathSample.rng = rng;

    vec4 lightRandSample = sample4f(rng);
    vec3 scatterRandSample = sample3f(rng);

    if ((uSettings.sampleMode != SampleModeBSDF) && !isBSDFDelta(mat)) {
        const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

        vec3 lightRadiance, lightDir;
        vec2 lightBary;
        float lightDist, lightPdf, lightJacobian;
        uint lightId;

        lightRadiance = sampleLight(surf.pos, lightDir, lightDist, lightPdf, lightJacobian, lightBary, lightId, lightRandSample);

        bool shadowed = traceShadow(
            uTLAS,
            shadowRayFlags, 0xff,
            surf.pos, MinRayDistance, lightDir, lightDist - MinRayDistance
        );

        if (!shadowed && lightPdf > 1e-6) {
            float bsdfPdf = evalPdf(mat, surf.norm, wo, lightDir);
            float weight = MISWeight(lightPdf, bsdfPdf);

            if (uSettings.sampleMode == SampleModeLight) {
                weight = 1.0;
            }
            vec3 contrib = lightRadiance * evalBSDF(mat, surf.albedo, surf.norm, wo, lightDir) * satDot(surf.norm, lightDir) / lightPdf * weight;
            float sampleWeight = luminance(contrib);

            if (isnan(sampleWeight) || sampleWeight < 0) {
                sampleWeight = 0;
            }
            pathSample.isec = Intersection(lightBary, 0, lightId);
            pathSample.Li = lightRadiance * weight;
            pathSample.jacobian = lightJacobian;
            pathSample.samplePdf = lightPdf;
            pathSample.isLightSample = true;

            DIReservoirAddSample(resv, pathSample, sampleWeight, sample1f(resvRng));
            radiance += contrib;
        }
    }
    BSDFSample s;
    pathSample.isLightSample = false;

    if ((uSettings.sampleMode != SampleModeLight) && sampleBSDF(mat, surf.albedo, surf.norm, wo, scatterRandSample, s) && s.pdf > 1e-6) {
        Intersection isec = traceClosestHit(
            uTLAS,
            gl_RayFlagsOpaqueEXT, 0xff,
            surf.pos, MinRayDistance, s.wi, MaxRayDistance
        );

        if (intersectionIsValid(isec)) {
            SurfaceInfo hit;
            loadSurfaceInfo(isec, hit);
            float cosTheta = -dot(s.wi, hit.norm);

            if (hit.isLight
#if !SAMPLE_LIGHT_DOUBLE_SIDE
                && cosTheta > 0
#endif
                ) {
                float dist = length(hit.pos - surf.pos);
                float sumPower = uLightSampleTable[0].prob;
                float lightPdf = luminance(hit.albedo) / sumPower * dist * dist / abs(cosTheta);
                float weight = MISWeight(s.pdf, lightPdf);

                if (uSettings.sampleMode == SampleModeBSDF || isSampleTypeDelta(s.type)) {
                    weight = 1.0;
                }
                float cosTerm = isSampleTypeDelta(s.type) ? 1.0 : satDot(surf.norm, s.wi);

                vec3 wi = normalize(hit.pos - surf.pos);
                vec3 contrib = hit.albedo * s.bsdf * cosTerm / s.pdf * weight;
                float sampleWeight = luminance(contrib);

                if (isnan(sampleWeight) || sampleWeight < 0) {    
                    sampleWeight = 0;
                }
                pathSample.isec = isec;
                pathSample.Li = hit.albedo * weight;
                pathSample.jacobian = abs(cosTheta) / square(dist);
                pathSample.samplePdf = s.pdf;
                pathSample.isLightSample = false;

                DIReservoirAddSample(resv, pathSample, luminance(contrib), sample1f(resvRng));
                radiance += contrib;
            }
        }
    }
    DIReservoirResetIfInvalid(resv);

    // This is importance sampling, not resampling
    if (resv.sampleCount > 0 && DIPathSampleIsValid(resv.pathSample) && resv.weight > 0) {
        resv.pathSample.Li *= resv.resampleWeight / resv.weight;
        resv.weight = resv.resampleWeight;
    }
    else {
        DIPathSampleInit(resv.pathSample);
        resv.weight = 0;
        resv.resampleWeight = 0;
    }
    resv.sampleCount = 1;

    return radiance;
}

void randomReplay(inout DIReservoir dstResv, SurfaceInfo dstSurf, DIReservoir srcResv, SurfaceInfo srcSurf, vec3 wo, inout uint rng) {
    Material dstMat = uMaterials[dstSurf.matIndex];

    BSDFSample s;

    DIReservoir replayResv;
    DIReservoirReset(replayResv);

    vec3 Li = sampleLi(dstSurf, dstMat, wo, srcResv.pathSample.rng, rng, replayResv);

    replayResv.resampleWeight = srcResv.resampleWeight;
    replayResv.sampleCount = srcResv.sampleCount;

    if (DIReservoirIsValid(replayResv)) {
        DIReservoirMerge(dstResv, replayResv, sample1f(rng));
    }
}

void reconnection(inout DIReservoir dstResv, SurfaceInfo dstSurf, DIReservoir srcResv, SurfaceInfo srcSurf, vec3 wo, inout uint rng) {
    Material dstMat = uMaterials[dstSurf.matIndex];
    float resampleWeight = 0;
    uint count = srcResv.sampleCount;
    //count = 1;

    SurfaceInfo rcSurf;
    loadSurfaceInfo(srcResv.pathSample.isec, rcSurf);

    float dist = distance(rcSurf.pos, dstSurf.pos);
    vec3 wi = normalize(rcSurf.pos - dstSurf.pos);
    float cosTheta = -dot(rcSurf.norm, wi);

    DIPathSample srcSample = srcResv.pathSample;

    if (cosTheta > 0) {
        const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;

        if (traceVisibility(uTLAS, shadowRayFlags, 0xff, dstSurf.pos, rcSurf.pos)) {
            float dstJacobian = abs(cosTheta) / square(dist);
            float jacobian = dstJacobian / srcSample.jacobian;

            float dstSamplePdf;

            if (srcSample.isLightSample) {
                float sumPower = uLightSampleTable[0].prob;
                dstSamplePdf = luminance(rcSurf.albedo) / sumPower / dstJacobian;
            }
            else {
                dstSamplePdf = evalPdf(dstMat, dstSurf.norm, wo, wi);
            }

            if (!isnan(jacobian) && srcSample.jacobian > 1e-6) {
                vec3 contrib = srcSample.Li * evalBSDF(dstMat, dstSurf.albedo, dstSurf.norm, wo, wi) * satDot(dstSurf.norm, wi) / srcSample.samplePdf;
                resampleWeight = luminance(contrib) * float(count);
            }
        }
    }
    if (isnan(resampleWeight) || resampleWeight < 0) {
        resampleWeight = 0;
    }
    //DIReservoirAddSample(dstResv, srcSample, resampleWeight, count, sample1f(rng));
}

void DIReservoirReuseAndMerge(inout DIReservoir dstResv, SurfaceInfo dstSurf, DIReservoir srcResv, SurfaceInfo srcSurf, vec3 wo, inout uint rng) {
    if (uSettings.shiftMode == Reconnection) {
        reconnection(dstResv, dstSurf, srcResv, srcSurf, wo, rng);
    }
    else if (uSettings.shiftMode == Replay) {
        randomReplay(dstResv, dstSurf, srcResv, srcSurf, wo, rng);
    }
}

#endif