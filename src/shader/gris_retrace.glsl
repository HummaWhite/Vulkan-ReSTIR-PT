#ifndef GRIS_RETRACE_GLSL
#define GRIS_RETRACE_GLSL

#include "camera.glsl"
#include "gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "material.glsl"
#include "gris_reservoir.glsl"

const uint Temporal = 0;
const uint Spatial = 1;

#define MAPPING_CHECK 0

/*
bool findPreviousReservoir(vec2 uv, vec3 pos, float depth, vec3 normal, vec3 albedo, int matMeshId, out GRISReservoir resv) {
    if (uv.x < 0 || uv.y < 0 || uv.x > 1.0 || uv.y > 1.0) {
        return false;
    }
    ivec2 pixelId = ivec2(uv * vec2(uCamera.filmSize.xy));

    float depthPrev;
    vec3 normalPrev;
    vec3 albedoPrev;
    int matMeshIdPrev;

    if (!unpackGBuffer(texture(uDepthNormalPrev, uv), texelFetch(uAlbedoMatIdPrev, pixelId, 0), depthPrev, normalPrev, albedoPrev, matMeshIdPrev)) {
        return false;
    }

    Ray ray = pinholeCameraSampleRay(uPrevCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));
    vec3 posPrev = ray.ori + ray.dir * (depthPrev - 1e-4);

    if (matMeshIdPrev != matMeshId || dot(normalPrev, normal) < 0.9 || abs(depthPrev - depth) > 5.0) {
        return false;
    }
    resv = uGRISReservoirPrev[index1D(uvec2(pixelId))];
    return true;
}
*/

void traceReplayPathForHybridShift(Intersection isec, SurfaceInfo surf, Ray ray, uint targetFlags, uint rng, out GRISReconnectionData rcData) {
    const int MaxTracingDepth = 15;

    vec3 throughput = vec3(1.0);
    vec3 wo = -ray.dir;
    bool rcPrevFound = false;

    Material mat = uMaterials[surf.matIndex];
    BSDFSample s;

    rcData.rcPrevThroughput = vec3(0.0);
    GRISReconnectionDataReset(rcData);

    uint targetId = GRISPathFlagsRcVertexId(targetFlags);
    uint targetType = GRISPathFlagsRcVertexType(targetFlags);

    #pragma unroll
    for (int bounce = 0; bounce < MaxTracingDepth; bounce++) {
        if (bounce > 0) {
            isec = traceClosestHit(
                uTLAS,
                gl_RayFlagsOpaqueEXT, 0xff,
                ray.ori, MinRayDistance, ray.dir, MaxRayDistance
            );

            if (!intersectionIsValid(isec)) {
                break;
            }
            loadSurfaceInfo(isec, surf);
            mat = uMaterials[surf.matIndex];
        }
        bool isThisVertexConnectible = isBSDFConnectible(mat);

        float resvRandSample = sample1f(rng);

#if MAPPING_CHECK
        if (rcPrevFound) {
            if (!(isThisVertexConnectible || surf.isLight) ||
                (!(targetType != RcVertexTypeSurface) && surf.isLight)) {
                intersectionSetInvalid(rcData.rcPrevIsec);
            }
            break;
        }
#endif
        if (surf.isLight) {
            break;
        }

        if (bounce == targetId - 1) {
            if (isThisVertexConnectible) {
                rcData.rcPrevIsec = isec;
                rcData.rcPrevWo = wo;
                rcData.rcPrevThroughput = throughput;
                rcPrevFound = true;
#if !MAPPING_CHECK
                break;
#endif
            }
            else {
                break;
            }
        }
        // make sure sample space is aligned for all paths
        vec4 lightRandSample = sample4f(rng);
        resvRandSample = sample1f(rng);

        if (bounce > 4) {
            float pdfTerminate = max(1.0 - luminance(throughput) * uSettings.rrScale, 0);

            if (sample1f(rng) < pdfTerminate) {
                break;
            }
            throughput /= (1.0 - pdfTerminate);
        }

        if (!sampleBSDF(mat, surf.albedo, surf.norm, wo, sample3f(rng), s) || s.pdf < 1e-6) {
            break;
        }

        float cosTheta = isSampleTypeDelta(s.type) ? 1.0 : absDot(surf.norm, s.wi);
        vec3 scatterTerm = s.bsdf * cosTheta / s.pdf;
        throughput *= scatterTerm;

        wo = -s.wi;
        ray.dir = s.wi;
        ray.ori = surf.pos + ray.dir * 1e-4;
    }
}

void GRISReservoirReuseAndMerge(inout GRISReservoir dstResv, SurfaceInfo dstPrimarySurf, Intersection dstPrimaryIsec, Ray primaryRay, GRISReservoir srcResv, SurfaceInfo srcPrimarySurf, inout uint rng) {
    GRISPathSample srcSample = srcResv.pathSample;
    GRISReconnectionData dstRcData;
    SurfaceInfo rcPrevSurf;
    SurfaceInfo rcSurf;
    Material rcMat;
    Material rcPrevMat;

    vec3 wi;
    vec3 Li = vec3(0.0);
    bool srcSampleValid = false;
    float dstJacobian = 0;
    float jacobian = 0;
    float dist = 0;
    float dstPHat = 0;
    float dstSamplePdf = 0;

    if (GRISPathSampleIsValid(srcSample)) {
        traceReplayPathForHybridShift(dstPrimaryIsec, dstPrimarySurf, primaryRay, srcSample.flags, srcSample.primaryRng, dstRcData);

        if (GRISReconnectionDataIsValid(dstRcData)) {
            loadSurfaceInfo(dstRcData.rcPrevIsec, rcPrevSurf);
            loadSurfaceInfo(srcSample.rcIsec, rcSurf);

            rcMat = uMaterials[rcSurf.matIndex];
            rcPrevMat = uMaterials[rcPrevSurf.matIndex];

            dist = distance(rcPrevSurf.pos, rcSurf.pos);
            wi = normalize(rcSurf.pos - rcPrevSurf.pos);

            float cosTheta = -dot(rcSurf.norm, wi);
            dstJacobian = abs(cosTheta) / square(dist);
            jacobian = dstJacobian / srcSample.rcJacobian;

            if (dist > GRISDistanceThreshold && cosTheta > 0 && !isnan(jacobian) && srcSample.rcJacobian > 0 &&
                isBSDFConnectible(rcPrevMat)) {
                const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;

                if (traceVisibility(uTLAS, shadowRayFlags, 0xff, rcPrevSurf.pos, rcSurf.pos)) {
                    srcSampleValid = true;
                }
            }
        }
    }

    if (srcSampleValid) {
        uint rcType = GRISPathFlagsRcVertexType(srcSample.flags);

        if (!isnan(srcSample.rcPrevSamplePdf) && srcSample.rcPrevSamplePdf > 1e-6) {
            Li = srcSample.rcLi;
            float pdf2 = 1.0;

            if (rcType == RcVertexTypeSurface && length(srcSample.rcWi) > 0.5) {
                Li *= evalBSDF(rcMat, rcSurf.albedo, rcSurf.norm, -wi, srcSample.rcWi) * satDot(rcSurf.norm, srcSample.rcWi);
                pdf2 = evalPdf(rcMat, rcSurf.norm, -wi, srcSample.rcWi);
            }
            Li *= evalBSDF(rcPrevMat, rcPrevSurf.albedo, rcPrevSurf.norm, dstRcData.rcPrevWo, wi) * satDot(rcPrevSurf.norm, wi);
            Li *= dstRcData.rcPrevThroughput;
            Li /= srcSample.rcPrevSamplePdf;

            if (!isBlack(Li) && !hasNan(Li)) {
                dstPHat = luminance(Li * jacobian);
            }

            if (rcType == RcVertexTypeLightSampled) {
                float sumPower = uLightSampleTable[0].prob;
                dstSamplePdf = luminance(rcSurf.albedo) / sumPower / dstJacobian;
            }
            else {
                dstSamplePdf = evalPdf(rcPrevMat, rcPrevSurf.norm, dstRcData.rcPrevWo, wi);
            }
        }
        float srcPHat = luminance(srcSample.F);

        srcSample.rcJacobian = dstJacobian;
        //srcSample.rcPrevSamplePdf /= jacobian;
        srcSample.rcPrevSamplePdf = dstSamplePdf;
        srcSample.F = Li;

        if (srcSample.rcPrevSamplePdf < 1e-6 || isnan(srcSample.rcPrevSamplePdf)) {
            srcSample.rcPrevSamplePdf = 0;
        }
        srcResv.pathSample = srcSample;
        srcResv.resampleWeight *= dstPHat / srcPHat;
    }
    else {
        srcResv.resampleWeight = 0;
    }

    if (GRISReservoirIsValid(srcResv)) {
        GRISReservoirMerge(dstResv, srcResv, sample1f(rng));
    }
    GRISReservoirCapSample(dstResv, uSettings.cap);
}

/*
vec3 retrace(uvec2 index, uvec2 frameSize) {
    vec2 uv = (vec2(index) + 0.5) / vec2(frameSize);

    float depth;
    vec3 norm;
    vec3 albedo;
    int matMeshId;

    if (!unpackGBuffer(texture(uDepthNormal, uv), texelFetch(uAlbedoMatId, ivec2(index), 0), depth, norm, albedo, matMeshId)) {
        return vec3(0.0);
    }
    vec2 motion = texelFetch(uMotionVector, ivec2(index), 0).xy;
    vec2 temporalUV = uv + motion;
    ivec2 temporalIndex = ivec2(temporalUV * vec2(frameSize));

    Ray ray = pinholeCameraSampleRay(uCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));

    SurfaceInfo surf;
    surf.pos = ray.ori + ray.dir * (depth - 1e-4);
    surf.norm = norm;
    surf.albedo = albedo;
    surf.isLight = false;
    surf.matIndex = matMeshId >> 16;

    GRISReservoir resv = uGRISReservoir[index1D(index)];
    GRISReservoir temporalResv;

    if (!findPreviousReservoir(temporalUV, surf.pos, depth, norm, albedo, matMeshId, temporalResv)) {
        return vec3(0.0);
    }
    GRISPathSample thisSample = resv.pathSample;
    GRISPathSample temporalSample = temporalResv.pathSample;

    if (!GRISPathSampleIsValid(temporalSample) || !GRISPathSampleIsValid(thisSample)) {
        return vec3(0.0);
    }

    GRISReconnectionData rcData, temporalRcData;

    // special case for primary hit - I can't make it to get instance and triangle id for first hit
    // so I recorded screen uv in isec.bary
    Intersection isec;
    isec.instanceIdx = SpecialHitIndex;
    isec.bary = uv;

    Intersection temporalIsec;
    temporalIsec.instanceIdx = SpecialHitIndex;
    temporalIsec.bary = temporalUV;

    float depthPrev;
    vec3 normalPrev;
    vec3 albedoPrev;
    int matMeshIdPrev;

    if (!unpackGBuffer(texture(uDepthNormalPrev, temporalUV), texelFetch(uAlbedoMatIdPrev, temporalIndex, 0), depthPrev, normalPrev, albedoPrev, matMeshIdPrev)) {
        return vec3(0.0);
    }

    Ray temporalRay = pinholeCameraSampleRay(uPrevCamera, vec2(temporalUV.x, 1.0 - temporalUV.y), vec2(0));

    SurfaceInfo temporalSurf;
    temporalSurf.pos = temporalRay.ori + temporalRay.dir * (depthPrev - 1e-4);
    temporalSurf.norm = normalPrev;
    temporalSurf.albedo = albedoPrev;
    temporalSurf.isLight = false;
    temporalSurf.matIndex = matMeshIdPrev >> 16;

    traceReplayPathForHybridShift(isec, surf, ray, temporalSample.flags, temporalSample.primaryRng, rcData);
    traceReplayPathForHybridShift(temporalIsec, temporalSurf, temporalRay, thisSample.flags, thisSample.primaryRng, temporalRcData);

    uGRISReconnectionData[index1D(index) * 2 + 0] = rcData;
    uGRISReconnectionData[index1D(index) * 2 + 1] = temporalRcData;

    return vec3(0.0);
    //return assert(rcData.rcPrevIsec.instanceIdx == SpecialHitIndex);
    //return vec3(rcData.rcPrevIsec.bary, 1.0);
    //return rcData.rcPrevWo;
    //return colorWheel(float(rcVertexId == 2));
    //return rcData.rcPrevThroughput;
    //return assert(GRISPathFlagsRcVertexType(temporalSample.flags) == 2);
}
*/

#endif