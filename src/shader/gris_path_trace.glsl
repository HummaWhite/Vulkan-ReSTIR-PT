#ifndef GRIS_PATH_TRACE_GLSL
#define GRIS_PATH_TRACE_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "material.glsl"
#include "gris_reservoir.glsl"

struct GRISTraceSettings {
	uint shiftMode;
	float rrScale;
};

layout(push_constant) uniform _PushConstant {
    GRISTraceSettings uSettings;
};

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
    uint rng = makeSeed(uCamera.seed + index.x, index.y);

    vec3 radiance = vec3(0.0);
    vec3 scatteredRadiance = vec3(0.0);
    vec3 throughput = vec3(1.0);
    vec3 rcThroughput;
    vec3 wo = -ray.dir;
    vec3 lastPos;
    bool isLastVertexConnectible = false;
    bool foundScattered = false;

    SurfaceInfo surf;
    surf.pos = ray.ori + ray.dir * (depth - 1e-4);
    surf.norm = norm;
    surf.albedo = albedo;
    surf.isLight = false;

    Material mat = uMaterials[matId];
    BSDFSample s;
    Intersection isec;

    GRISPathSample pathSample;
    GRISPathSample tempSample;
    GRISPathSampleReset(pathSample);
    GRISPathSampleReset(tempSample);
    pathSample.primaryRng = rng;
    tempSample.primaryRng = rng;

    // GRISReservoir resv = uGRISReservoirPrev[index1D(index)];
    GRISReservoir resv;
    resv.resampleWeight = 0;
    resv.sampleCount = 0;
    GRISPathSampleReset(resv.pathSample);

    #pragma unroll
    for (uint bounce = 0; bounce < MaxTracingDepth; bounce++) {
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

        float cosPrevWi = dot(ray.dir, surf.norm);
        float distToPrev = distance(lastPos, surf.pos);
        float geometryJacobian = abs(cosPrevWi) / square(distToPrev);
        bool isThisVertexConnectible = surf.isLight || isBSDFConnectible(mat);

        bool recordScatteredRcVertex = (uSettings.shiftMode == Reconnection && bounce == 1 && !surf.isLight) ||
            (!foundScattered && isLastVertexConnectible && isThisVertexConnectible && distToPrev > GRISDistanceThreshold);

        float scatterResvRandSample = sample1f(rng);

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
                vec3 weightedLs = surf.albedo * weight;
                radiance += weightedLs * throughput;

                if (recordScatteredRcVertex && !foundScattered) {
                    tempSample.rcIsec = isec;
                    tempSample.rcRng = rng;
                    tempSample.rcPrevSamplePdf = s.pdf;
                    tempSample.rcJacobian = geometryJacobian;
                    tempSample.rcLi = weightedLs;
                    tempSample.F = weightedLs * throughput;

                    GRISPathFlagsSetRcVertexId(tempSample.flags, bounce);
                    GRISPathFlagsSetPathLength(tempSample.flags, bounce + 1);
                    GRISPathFlagsSetRcVertexType(tempSample.flags, RcVertexTypeLightScattered);

                    GRISReservoirAddSample(resv, tempSample, luminance(tempSample.F), scatterResvRandSample);
                    break;
                }
                else if (recordScatteredRcVertex || foundScattered) {
                    pathSample.rcLs += weightedLs * rcThroughput;
                    scatteredRadiance += weightedLs * throughput;
                }
            }
        }

        if (recordScatteredRcVertex) {
            foundScattered = true;

            pathSample.rcIsec = isec;
            pathSample.rcRng = rng;
            pathSample.rcPrevSamplePdf = s.pdf;
            pathSample.rcJacobian = geometryJacobian;
            pathSample.rcLi = surf.isLight ? surf.albedo : vec3(0.0);

            GRISPathFlagsSetRcVertexId(pathSample.flags, bounce);
            GRISPathFlagsSetPathLength(pathSample.flags, bounce + 1);
            GRISPathFlagsSetRcVertexType(pathSample.flags, surf.isLight ? RcVertexTypeLightScattered : RcVertexTypeSurface);
        }

        if (surf.isLight) {
            break;
        }
        vec4 lightRandSample = sample4f(rng);
        float lightResvRandSample = sample1f(rng);

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

            // if light sampling successful, mark sample point as rcVertex
            bool recordLightRcVertex = (!foundScattered && lightDist > GRISDistanceThreshold && (uSettings.shiftMode != Reconnection));

            if (!shadowed && lightPdf > 1e-6 && !isBlack(lightRadiance)) {
                float bsdfPdf = absDot(surf.norm, lightDir) * PiInv;
                float weight = MISWeight(lightPdf, bsdfPdf);
                vec3 scatterTerm = evalBSDF(mat, surf.albedo, surf.norm, wo, lightDir) * satDot(surf.norm, lightDir);
                vec3 weightedLi = lightRadiance / lightPdf * weight;

                if (recordLightRcVertex) {
                    vec3 L = weightedLi * scatterTerm * throughput;

                    tempSample.rcIsec.instanceIdx = 0;
                    tempSample.rcIsec.triangleIdx = lightId;
                    tempSample.rcIsec.bary = lightBary;

                    tempSample.rcRng = rng;
                    tempSample.rcPrevSamplePdf = lightPdf;
                    tempSample.rcJacobian = lightJacobian;
                    tempSample.rcLi = lightRadiance;
                    tempSample.F = L;

                    GRISPathFlagsSetRcVertexId(tempSample.flags, bounce + 1);
                    GRISPathFlagsSetPathLength(tempSample.flags, bounce + 2);
                    GRISPathFlagsSetRcVertexType(tempSample.flags, RcVertexTypeLightSampled);

                    GRISReservoirAddSample(resv, tempSample, luminance(L), lightResvRandSample);
                }

                if (foundScattered) {
                    if (bounce == GRISPathFlagsRcVertexId(pathSample.flags)) {
                        pathSample.rcLi = weightedLi;
                        pathSample.rcWi = lightDir;
                    }
                    else {
                        pathSample.rcLs += weightedLi * scatterTerm * rcThroughput;
                    }
                    scatteredRadiance += weightedLi * scatterTerm * throughput;
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

        if (foundScattered) {
            if (bounce == GRISPathFlagsRcVertexId(pathSample.flags)) {
                pathSample.rcWs = s.wi;
                rcThroughput = vec3(1.0) / s.pdf;
            }
            else {
                rcThroughput *= scatterTerm;
            }
        }

        lastPos = surf.pos;
        wo = -s.wi;
        ray.dir = s.wi;
        ray.ori = surf.pos + ray.dir * 1e-4;
        isLastVertexConnectible = isThisVertexConnectible;
    }

    if (foundScattered) {
        pathSample.F = scatteredRadiance;
        GRISReservoirAddSample(resv, pathSample, luminance(pathSample.F), sample1f(rng));
    }
    GRISReservoirResetIfInvalid(resv);
    
    if (uSettings.shiftMode == Reconnection) {
        GRISPathFlagsSetRcVertexType(resv.pathSample.flags, RcVertexTypeSurface);
    }

    if (!GRISPathSampleIsValid(resv.pathSample)) {
        GRISPathSampleReset(resv.pathSample);
    }

    uGRISReservoir[index1D(index)] = resv;
    pathSample = resv.pathSample;

    uint pathLength = GRISPathFlagsPathLength(pathSample.flags);
    uint rcVertexId = GRISPathFlagsRcVertexId(pathSample.flags);
    uint rcVertexType = GRISPathFlagsRcVertexType(pathSample.flags);

    //radiance = scatteredRadiance;
    //radiance = colorWheel(resv.sampleCount / 2.0);
    //radiance = colorWheel(float(foundScattered));
    //radiance = colorWheel(float(rcVertexId) / float(pathLength));
    //radiance = colorWheel(float(pathLength) / float(MaxTracingDepth));
    //radiance = colorWheel(float(rcVertexId) / 6.0);
    //radiance = vec3(pathSample.rcPrevScatterPdf);
    //radiance = vec3(pathSample.rcJacobian);
    //radiance = colorWheel(pathSample.rcJacobian);
    //radiance = pathSample.rcLi;
    //radiance = pathSample.rcLs;
    //radiance = vec3(resv.resampleWeight / resv.sampleCount);
    //radiance = colorWheel(resv.sampleCount / 3);
    //radiance = assert(rcVertexId == 2);
    //radiance = assert(rcVertexType == RcVertexTypeLightScattered);
    radiance = pathSample.F / luminance(pathSample.F) * resv.resampleWeight;
    //radiance -= pathSample.F / luminance(pathSample.F) * resv.resampleWeight;
    //radiance = assert(foundScattered);

    return radiance;
}

#endif