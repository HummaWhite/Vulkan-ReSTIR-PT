#ifndef GRIS_PATH_TRACE_GLSL
#define GRIS_PATH_TRACE_GLSL

#include "camera.glsl"
#include "gbuffer_util.glsl"
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

vec3 indirectIllumination(uvec2 index, uvec2 frameSize) {
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
    vec3 throughput = vec3(1.0);
    vec3 rcThroughput = vec3(1.0);
    vec3 wo = -ray.dir;
    vec3 lastPos;
    bool isLastVertexConnectible = false;
    bool rcVertexFound = false;
    uint rcVertexId = 0;

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
        GRISPathFlagsSetPathLength(pathSample.flags, bounce + 1);

        if (bounce > 0) {
            isec = traceClosestHit(
                uTLAS,
                gl_RayFlagsOpaqueEXT, 0xff,
                ray.ori, MinRayDistance, ray.dir, MaxRayDistance
            );

            if (!IntersectionIsValid(isec)) {
                break;
            }
            loadSurfaceInfo(isec, surf);
            mat = uMaterials[surf.matIndex];
        }

        float cosPrevWi = dot(ray.dir, surf.norm);
        float distToPrev = distance(lastPos, surf.pos);
        float geometryJacobian = abs(cosPrevWi) / square(distToPrev);

        if (surf.isLight) {
            if (/* cosPrevWi < 0 */ true) {
                float weight = 1.0;
                float sumPower = uLightSampleTable[0].prob;
                float lightPdf = luminance(surf.albedo) / sumPower / geometryJacobian;

                if (bounce > 0 && !isSampleTypeDelta(s.type)) {
                    weight = MISWeight(s.pdf, lightPdf);
                }
                vec3 weightedLi = surf.albedo * weight;

                if (rcVertexFound) {
                    pathSample.rcLs += weightedLi * rcThroughput;
                }
                radiance += weightedLi * throughput;
            }
        }
        bool isThisVertexConnectible = surf.isLight || isBSDFConnectible(mat);

        bool recordRcVertex = (uSettings.shiftMode == Reconnection && bounce == 1) ||
            (!rcVertexFound && isLastVertexConnectible && isThisVertexConnectible && distToPrev > GRISDistanceThreshold);

        if  (recordRcVertex) {
            rcVertexFound = true;
            rcVertexId = bounce;

            pathSample.rcIsec = isec;
            pathSample.rcRng = rng;
            pathSample.rcPrevScatterPdf = s.pdf;
            pathSample.rcJacobian = geometryJacobian;
            pathSample.rcLi = surf.isLight ? surf.albedo : vec3(0.0);
            pathSample.rcIsLight = surf.isLight;

            GRISPathFlagsSetRcVertexId(pathSample.flags, rcVertexId);
        }

        if (surf.isLight) {
            break;
        }
        vec4 lightRandSample = sample4f(rng);

        if (/* sample direct lighting */ bounce > 0 && !isBSDFDelta(mat)) {
            const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;

            vec3 lightRadiance, lightDir;
            float lightDist, lightPdf;

            lightRadiance = sampleLight(surf.pos, lightDir, lightDist, lightPdf, lightRandSample);

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
                
                if (bounce == rcVertexId) {
                    pathSample.rcLi = weightedLi * rcThroughput;
                    pathSample.rcWi = lightDir;
                }
                else {
                    pathSample.rcLs += weightedLi * scatterTerm * rcThroughput;
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

        if (rcVertexFound) {
            if (bounce == rcVertexId) {
                pathSample.rcWs = s.wi;
                rcThroughput /= s.pdf;
            }
            else {
                rcThroughput *= scatterTerm;
            }
        }
        else {
            rcThroughput /= s.pdf;
        }

        lastPos = surf.pos;
        wo = -s.wi;
        ray.dir = s.wi;
        ray.ori = surf.pos + ray.dir * 1e-4;
        isLastVertexConnectible = isThisVertexConnectible;
    }

    if (true) {
        if (GRISPathSampleIsValid(pathSample)) {
            float sampleWeight = luminance(radiance);

            if (isnan(sampleWeight) || sampleWeight < 0) {
                sampleWeight = 0;
            }
            GRISReservoirAddSample(resv, pathSample, sampleWeight, sample1f(rng));
        }

        GRISReservoirResetIfInvalid(resv);
        GRISReservoirCapSample(resv, 40);
    }
    uGRISReservoir[index1D(index)] = resv;

    uint pathLength = GRISPathFlagsPathLength(pathSample.flags);

    //radiance = colorWheel(float(rcVertexId) / float(pathLength));
    //radiance = colorWheel(float(rcVertexId) / 8.0);
    //radiance = vec3(GRISPathFlagsRcVertexId(pathSample.flags) == 1);
    //radiance = vec3(pathSample.rcPrevScatterPdf);
    //radiance = vec3(pathSample.rcJacobian);
    //radiance = colorWheel(pathSample.rcJacobian);
    //radiance = pathSample.rcLi;
    //radiance = pathSample.rcLs;
    //radiance = colorWheel(float(pathSample.rcIsLight));
    //radiance = vec3(resv.resampleWeight / resv.sampleCount);

    return clampColor(radiance);
}

#endif