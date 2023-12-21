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

    vec3 lightSampledRadiance = vec3(0.0);
    vec3 scatteredRadiance = vec3(0.0);
    vec3 radiance = vec3(0.0);
    float lightSampledWeight = 0;
    vec3 throughput = vec3(1.0);
    vec3 rcThroughput;
    vec3 wo = -ray.dir;
    vec3 lastPos;
    bool isLastVertexConnectible = false;
    bool foundScattered = false;
    bool lightSampled = (uSettings.shiftMode == Reconnection);

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
    resv.resampleWeight = 0;
    resv.sampleCount = 0;

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

        bool recordScatteredRcVertex = (uSettings.shiftMode == Reconnection && bounce == 1 && !surf.isLight) ||
            (!foundScattered && lightSampled && isLastVertexConnectible && isThisVertexConnectible && distToPrev > GRISDistanceThreshold);

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

                if (recordScatteredRcVertex || foundScattered) {
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
            GRISPathFlagsSetRcVertexType(pathSample.flags, surf.isLight ? RcVertexTypeLightScattered : RcVertexTypeSurface);
        }

        if (surf.isLight) {
            break;
        }
        vec4 lightRandSample = sample4f(rng);

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
            bool recordLightRcVertex = (!lightSampled && isThisVertexConnectible && lightDist > GRISDistanceThreshold && (uSettings.shiftMode != Reconnection));

            if (!shadowed && lightPdf > 1e-6) {
                float bsdfPdf = absDot(surf.norm, lightDir) * PiInv;
                float weight = MISWeight(lightPdf, bsdfPdf);
                vec3 scatterTerm = evalBSDF(mat, surf.albedo, surf.norm, wo, lightDir) * satDot(surf.norm, lightDir);
                vec3 weightedLi = lightRadiance / lightPdf * weight;

                if (recordLightRcVertex) {
                    lightSampled = true;
                    pathSample.rcIsec.instanceIdx = 0;
                    pathSample.rcIsec.triangleIdx = lightId;
                    pathSample.rcIsec.bary = lightBary;

                    pathSample.rcRng = rng;
                    pathSample.rcPrevSamplePdf = lightPdf;
                    pathSample.rcJacobian = lightJacobian;
                    pathSample.rcLi = lightRadiance;

                    GRISPathFlagsSetRcVertexId(pathSample.flags, bounce + 1);
                    GRISPathFlagsSetRcVertexType(pathSample.flags, RcVertexTypeLightSampled);

                    lightSampledRadiance = lightRadiance / lightPdf * scatterTerm * throughput;
                    lightSampledWeight = weight;

                    GRISReservoirInitSample(resv, pathSample, luminance(lightSampledRadiance * weight));
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
        GRISReservoirAddSample(resv, pathSample, luminance(scatteredRadiance), sample1f(rng));
    }

    if (false) {
        if (GRISPathSampleIsValid(pathSample)) {
            float sampleWeight = luminance(scatteredRadiance);

            if (isnan(sampleWeight) || sampleWeight < 0) {
                sampleWeight = 0;
            }
            GRISReservoirAddSample(resv, pathSample, sampleWeight, sample1f(rng));
        }

        GRISReservoirResetIfInvalid(resv);
        GRISReservoirCapSample(resv, 40);
    }
    GRISReservoirResetIfInvalid(resv);
    
    if (uSettings.shiftMode == Reconnection) {
        GRISPathFlagsSetRcVertexType(resv.pathSample.flags, RcVertexTypeSurface);
    }

    if (resv.sampleCount != 0) {
        resv.sampleCount = 1;
    }
    uGRISReservoir[index1D(index)] = resv;

    pathSample = resv.pathSample;

    if (resv.sampleCount < 0.5 || !GRISPathSampleIsValid(pathSample)) {
        GRISPathSampleReset(pathSample);
    }
    uint pathLength = GRISPathFlagsPathLength(pathSample.flags);
    uint rcVertexId = GRISPathFlagsRcVertexId(pathSample.flags);

    //radiance = vec3(resv.resampleWeight / (resv.sampleCount + 0.001));
    //radiance = lightSampledRadiance * lightSampledWeight + scatteredRadiance;
    //radiance = lightSampledRadiance * lightSampledWeight;
    //radiance = scatteredRadiance;
    //radiance = colorWheel(resv.sampleCount / 2.0);
    //radiance = colorWheel(float(foundScattered));
    //radiance = colorWheel(float(rcVertexId) / float(pathLength));
    //radiance = colorWheel(float(rcVertexId) / 4.0);
    //radiance = vec3(pathSample.rcPrevScatterPdf);
    //radiance = vec3(pathSample.rcJacobian);
    //radiance = colorWheel(pathSample.rcJacobian);
    //radiance = pathSample.rcLi;
    //radiance = pathSample.rcLs;
    //radiance = colorWheel(float(pathLength == 3 && isLastVertexConnectible));
    //radiance = colorWheel(float(pathSample.rcIsLight));
    //radiance = vec3(resv.resampleWeight / resv.sampleCount);
    radiance = assert(rcVertexId == 1);

    return radiance;
}

#endif