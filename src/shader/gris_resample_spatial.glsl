#ifndef GRIS_RESAMPLE_SPATIAL_GLSL
#define GRIS_RESAMPLE_SPATIAL_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "math.glsl"
#include "gris_reservoir.glsl"
#include "gris_retrace.glsl"

bool findNeighborReservoir(vec2 uv, vec3 pos, float depth, vec3 normal, vec3 albedo, int matMeshId, out GRISReservoir resv, out SurfaceInfo surf) {
    if (uv.x < 0 || uv.y < 0 || uv.x > 1.0 || uv.y > 1.0) {
        return false;
    }
    ivec2 pixelId = ivec2(uv * vec2(uCamera.filmSize.xy));

    float depthPrev;
    vec3 normalPrev;
    vec3 albedoPrev;
    int matMeshIdPrev;

    if (!unpackGBuffer(texture(uDepthNormal, uv), texelFetch(uAlbedoMatId, pixelId, 0), depthPrev, normalPrev, albedoPrev, matMeshIdPrev)) {
        return false;
    }
    Ray ray = pinholeCameraSampleRay(uCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));
    vec3 posPrev = ray.ori + ray.dir * (depthPrev - 1e-4);

    if (dot(normalPrev, normal) < 0.9 || distance(pos, posPrev) > 0.4) {
        return false;
    }
    resv = uGRISReservoirTemp[index1D(uvec2(pixelId))];
    surf = SurfaceInfo(posPrev, normalPrev, albedoPrev, matMeshIdPrev >> 16, false);
    return true;
}

vec3 spatialReuse(uvec2 index, uvec2 frameSize) {
    vec2 uv = (vec2(index) + 0.5) / vec2(frameSize);

    float depth;
    vec3 norm;
    vec3 albedo;
    int matMeshId;

    if (!unpackGBuffer(texture(uDepthNormal, uv), texelFetch(uAlbedoMatId, ivec2(index), 0), depth, norm, albedo, matMeshId)) {
        return vec3(0.0);
    }
    uint rng = makeSeed(uCamera.seed, index) ^ 2;

    Ray ray = pinholeCameraSampleRay(uCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));

    vec3 pos = ray.ori + ray.dir * (depth - 1e-4);
    int matId = matMeshId >> 16;

    vec3 radiance = vec3(0.0);

    GRISReservoir resv = uGRISReservoirTemp[index1D(index)];

    Intersection dstPrimaryIsec;
    dstPrimaryIsec.instanceIdx = SpecialHitIndex;
    dstPrimaryIsec.bary = uv;

    SurfaceInfo dstPrimarySurf = SurfaceInfo(pos, norm, albedo, matId, false);

    if (uSettings.spatialReuse) {
        const uint ResampleNum = 3;
        const float ResampleRadius = 20.0;
        vec2 texelSize = 1.0 / vec2(frameSize);
        SurfaceInfo srcPrimarySurf;

#pragma unroll
        for (uint i = 0; i < ResampleNum; i++) {
            vec2 neighbor = uv + toConcentricDisk(sample2f(rng)) * ResampleRadius * texelSize;

            GRISReservoir neighborResv;
            GRISReservoirReset(neighborResv);

            if (findNeighborReservoir(neighbor, pos, depth, norm, albedo, matMeshId, neighborResv, srcPrimarySurf)) {
                if (GRISReservoirIsValid(neighborResv)) {
                    GRISReservoirReuseAndMerge(resv, dstPrimarySurf, dstPrimaryIsec, ray, neighborResv, srcPrimarySurf, rng);
                }
            }
        }
    }
    GRISReservoirResetIfInvalid(resv);

    uGRISReservoir[index1D(index)] = resv;

    if (GRISReservoirIsValid(resv) && resv.sampleCount > 0) {
        
        GRISPathSample pathSample = resv.pathSample;

        if (false) {
            radiance = pathSample.F / luminance(pathSample.F) * resv.resampleWeight / resv.sampleCount;
        }
        if (GRISPathSampleIsValid(pathSample)) {
            GRISReconnectionData rcData;
            traceReplayPathForHybridShift(dstPrimaryIsec, dstPrimarySurf, ray, pathSample.flags, pathSample.primaryRng, rcData);

            if (GRISReconnectionDataIsValid(rcData)) {
                SurfaceInfo rcPrevSurf;
                SurfaceInfo rcSurf;

                if (intersectionIsSpecial(rcData.rcPrevIsec)) {
                    rcPrevSurf = dstPrimarySurf;
                }
                else {
                    loadSurfaceInfo(rcData.rcPrevIsec, rcPrevSurf);
                }
                loadSurfaceInfo(pathSample.rcIsec, rcSurf);

                Material rcMat = uMaterials[rcSurf.matIndex];
                Material rcPrevMat = uMaterials[rcPrevSurf.matIndex];

                vec3 Li = pathSample.rcLi;

                uint rcType = GRISPathFlagsRcVertexType(pathSample.flags);

                vec3 wi = normalize(rcSurf.pos - rcPrevSurf.pos);

                if (rcType == RcVertexTypeSurface && length(pathSample.rcWi) > 0.5) {
                    Li *= evalBSDF(rcMat, rcSurf.albedo, rcSurf.norm, -wi, pathSample.rcWi) * satDot(rcSurf.norm, pathSample.rcWi);
                }
                Li *= evalBSDF(rcPrevMat, rcPrevSurf.albedo, rcPrevSurf.norm, rcData.rcPrevWo, wi) * satDot(rcPrevSurf.norm, wi);
                Li *= rcData.rcPrevThroughput;
                Li /= pathSample.rcPrevSamplePdf;

                if (!isBlack(Li) && !hasNan(Li)) {
                    radiance = Li / luminance(Li) * resv.resampleWeight / resv.sampleCount;
                }
            }
        }
        
    }
    return clampColor(radiance);
}

#endif