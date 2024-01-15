#ifndef GRIS_PATH_TRACE_GLSL
#define GRIS_PATH_TRACE_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "material.glsl"
#include "gris_reservoir.glsl"

struct StreamSampler {
    GRISPathSample pathSample;
    float weight;
    float sumWeight;
    uint sampleCount;
};

StreamSampler initStream() {
    StreamSampler r;
    r.weight = 0;
    r.sumWeight = 0;
    r.sampleCount = 0;
    return r;
}

void addStream(inout StreamSampler resv, GRISPathSample pathSample, float weight, float r) {
    resv.sumWeight += weight;

    if (r * resv.sumWeight < weight) {
        resv.weight = weight;
        resv.pathSample = pathSample;
        resv.sampleCount++;
    }
}

uint nextRcVertexSampleState(uint state, bool connectible) {
    if (state == 2) {
        return 2;
    }
    if (!connectible) {
        return 0;
    }
    return state + uint(connectible);
}

vec3 tracePath(uvec2 index, uvec2 frameSize) {
    const int MaxTracingDepth = 15;
    vec2 uv = (vec2(index) + 0.5) / vec2(frameSize);

    float depth;
    vec3 norm;
    vec3 albedo;
    int matMeshId;

    if (!unpackGBuffer(texture(uDepthNormal, uv), texelFetch(uAlbedoMatId, ivec2(index), 0), depth, norm, albedo, matMeshId)) {
        return vec3(0.0);
    }
    int matId = matMeshId >> 16;
    
    Ray ray = pinholeCameraSampleRay(uCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));
    uint rng = makeSeed(uCamera.seed, index);

    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);
    vec3 rcThroughput;
    vec3 wo = -ray.dir;
    vec3 lastPos;
    bool isLastVertexConnectible = false;
    uint sampleState = 0;
    uint lastSampleState = 0;

    SurfaceInfo surf;
    surf.pos = ray.ori + ray.dir * (depth - 1e-4);
    surf.norm = norm;
    surf.albedo = albedo;
    surf.isLight = false;

    Material mat = uMaterials[matId];
    BSDFSample s;
    Intersection isec;

    GRISPathSample pathSample;
    GRISPathSampleReset(pathSample);
    pathSample.primaryRng = rng;

    // GRISReservoir resv = uGRISReservoirPrev[index1D(index)];
    GRISReservoir resv;
    GRISReservoirReset(resv);
    StreamSampler streamResv = initStream();

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
        GRISPathFlagsSetPathLength(pathSample.flags, bounce + 1);

        float cosPrevWi = dot(ray.dir, surf.norm);
        float distToPrev = distance(lastPos, surf.pos);
        float geometryJacobian = abs(cosPrevWi) / square(distToPrev);

        bool isThisVertexConnectible = surf.isLight || isBSDFConnectible(mat);

        lastSampleState = sampleState;
        sampleState = nextRcVertexSampleState(sampleState, isThisVertexConnectible);

        if (uSettings.shiftMode == Reconnection && bounce == 1 && !surf.isLight) {
            sampleState = 2;
            lastSampleState = 1;
        }
        float resvRandSample = sample1f(rng);

        if (surf.isLight) {
            if (bounce > 1
#if !SAMPLE_LIGHT_DOUBLE_SIDE
                && cosPrevWi < 0
#endif
                ) {
                float weight = 1.0;
                float sumPower = uLightSampleTable[0].prob;
                float lightPdf = luminance(surf.albedo) / sumPower / geometryJacobian;

                if (!isSampleTypeDelta(s.type)) {
                    weight = MISWeight(s.pdf, lightPdf);
                }
                vec3 weightedLi = surf.albedo * weight;

                if (sampleState == 2 && lastSampleState == 2) {
                    pathSample.rcLi += weightedLi * rcThroughput;
                    pathSample.F += weightedLi * throughput;
                }
                else if ((sampleState == 2 && lastSampleState == 1) && isLastVertexConnectible && distToPrev > GRISDistanceThreshold) {
                    pathSample.rcIsec = isec;
                    pathSample.rcRng = rng;
                    pathSample.rcPrevSamplePdf = s.pdf;
                    pathSample.rcJacobian = geometryJacobian;
                    pathSample.rcLi = weightedLi;
                    pathSample.rcWi = vec3(0.0);
                    pathSample.F = weightedLi * throughput;

                    GRISPathFlagsSetRcVertexId(pathSample.flags, bounce);
                    GRISPathFlagsSetRcVertexType(pathSample.flags, RcVertexTypeLightScattered);
                    addStream(streamResv, pathSample, luminance(pathSample.F), resvRandSample);
                }
                radiance += weightedLi * throughput;
            }
            break;
        }

        bool connectible = (isThisVertexConnectible && isLastVertexConnectible && distToPrev > GRISDistanceThreshold);

        if ((sampleState == 2 && lastSampleState == 1) && (connectible || uSettings.shiftMode == Reconnection)) {
            pathSample.rcIsec = isec;
            pathSample.rcRng = rng;
            pathSample.rcPrevSamplePdf = s.pdf;
            pathSample.rcJacobian = geometryJacobian;

            GRISPathFlagsSetRcVertexId(pathSample.flags, bounce);
            GRISPathFlagsSetRcVertexType(pathSample.flags, RcVertexTypeSurface);

            rcThroughput = vec3(1.0);
        }

        vec4 lightRandSample = sample4f(rng);
        resvRandSample = sample1f(rng);

        if (/* sample direct lighting */ bounce > 0 && !isBSDFDelta(mat)) {
            const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;

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
                float bsdfPdf = absDot(surf.norm, lightDir) * PiInv;
                float weight = MISWeight(lightPdf, bsdfPdf);
                vec3 scatterTerm = evalBSDF(mat, surf.albedo, surf.norm, wo, lightDir) * satDot(surf.norm, lightDir);
                vec3 weightedLi = lightRadiance / lightPdf * weight;

                if (sampleState == 2 && lastSampleState == 2) {
                    pathSample.rcLi += weightedLi * scatterTerm * rcThroughput;
                    pathSample.F += weightedLi * scatterTerm * throughput;
                }
                else if (sampleState == 2 && lastSampleState == 1) {
                    pathSample.rcLi = weightedLi;
                    pathSample.rcWi = lightDir;
                    pathSample.F = weightedLi * scatterTerm * throughput;
                    addStream(streamResv, pathSample, luminance(pathSample.F), resvRandSample);
                }
                else if ((sampleState == 1) && isThisVertexConnectible && lightDist > GRISDistanceThreshold) {
                    pathSample.rcIsec = Intersection(lightBary, 0, lightId);
                    pathSample.rcRng = rng;
                    pathSample.rcPrevSamplePdf = lightPdf;
                    pathSample.rcJacobian = lightJacobian;
                    pathSample.rcLi = lightRadiance * weight;
                    pathSample.rcWi = vec3(0.0);
                    pathSample.F = weightedLi * scatterTerm * throughput;

                    GRISPathFlagsSetRcVertexId(pathSample.flags, bounce + 1);
                    GRISPathFlagsSetRcVertexType(pathSample.flags, RcVertexTypeLightSampled);
                    addStream(streamResv, pathSample, luminance(pathSample.F), resvRandSample);
                }
                radiance += weightedLi * scatterTerm * throughput;
            }
        }

        if (/* russian roulette */ bounce > 4) {
            float pdfTerminate = max(1.0 - luminance(throughput) * uSettings.rrScale, 0);

            if (sample1f(rng) < pdfTerminate) {
                break;
            }
            throughput /= (1.0 - pdfTerminate);
            rcThroughput /= (1.0 - pdfTerminate);
        }

        if (!sampleBSDF(mat, surf.albedo, surf.norm, wo, sample3f(rng), s) || s.pdf < 1e-6) {
            break;
        }

        float cosTheta = isSampleTypeDelta(s.type) ? 1.0 : absDot(surf.norm, s.wi);
        vec3 scatterTerm = s.bsdf * cosTheta / s.pdf;
        throughput *= scatterTerm;

        if (sampleState == 2 && lastSampleState == 2) {
            rcThroughput *= scatterTerm;
        }
        else if (sampleState == 2 && lastSampleState == 1) {
            pathSample.rcLi = vec3(0.0);
            pathSample.rcWi = s.wi;
            pathSample.F = vec3(0.0);
            rcThroughput /= s.pdf;
        }

        lastPos = surf.pos;
        wo = -s.wi;
        ray.dir = s.wi;
        ray.ori = surf.pos + ray.dir * 1e-4;
        isLastVertexConnectible = isThisVertexConnectible;
    }

    if (sampleState == 2 && lastSampleState == 2) {
        addStream(streamResv, pathSample, luminance(pathSample.F), sample1f(rng));
    }

    pathSample = streamResv.pathSample;

    if (streamResv.sumWeight > 0 && streamResv.weight > 0) {
        pathSample.F *= streamResv.sumWeight / streamResv.weight;
        pathSample.rcLi *= streamResv.sumWeight / streamResv.weight;
        resv.resampleWeight = luminance(pathSample.F);
    }
    else {
        pathSample.rcIsec.instanceIdx = InvalidHitIndex;
        pathSample.F = vec3(0);
    }
    resv.pathSample = pathSample;
    resv.sampleCount = 1;

    uGRISReservoir[index1D(index)] = resv;

    uint pathLength = GRISPathFlagsPathLength(pathSample.flags);
    uint rcVertexId = GRISPathFlagsRcVertexId(pathSample.flags);
    uint type = GRISPathFlagsRcVertexType(pathSample.flags);

    if (!GRISPathSampleIsValid(pathSample)) {
        return vec3(0.0);
    }

    //radiance = pathSample.F;
    //radiance = pathSample.rcLi;
    //radiance = colorWheel(float(streamResv.sampleCount) / 6.0);
    //radiance = colorWheel(float(rcVertexId) / float(pathLength));
    //radiance = colorWheel(float(rcVertexId) / 6.0);
    //radiance = colorWheel(float(type) / 2.0);
    //radiance = assert(type == RcVertexTypeLightSampled);
    //radiance = assert(type == RcVertexTypeLightScattered);
    //radiance = assert(type == RcVertexTypeSurface);
    //radiance = colorWheel(float(pathSample.rcIsec.instanceIdx) / 10.0);
    //radiance = vec3(pathSample.rcPrevSamplePdf);
    //radiance = vec3(pathSample.rcJacobian);
    //radiance = colorWheel(pathSample.rcJacobian);
    //radiance = colorWheel(float(pathLength == 3 && isLastVertexConnectible));
    //radiance = vec3(resv.resampleWeight / resv.sampleCount);
    //radiance = assert(rcVertexId == 1);

    return radiance;
}

#endif