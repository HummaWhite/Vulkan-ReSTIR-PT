#ifndef GRIS_RETRACE_GLSL
#define GRIS_RETRACE_GLSL

#include "camera.glsl"
#include "gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "material.glsl"
#include "gris_reservoir.glsl"

const uint Temporal = 0;
const uint Spatial = 1;

struct GRISRetraceSettings {
    uint shiftMode;
    float rrScale;
};

layout(push_constant) uniform _PushConstant {
    GRISRetraceSettings uSettings;
};

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

void traceReplayPathForHybridShift(Intersection isec, SurfaceInfo surf, Ray ray, uint targetRcId, uint rng, out GRISReconnectionData rcData) {
    const int MaxTracingDepth = 15;

    vec3 throughput = vec3(1.0);
    vec3 wo = -ray.dir;
    bool rcPrevFound = false;

    Material mat = uMaterials[surf.matIndex];
    BSDFSample s;

    rcData.rcPrevThroughput = vec3(0.0);
    IntersectionSetInvalid(rcData.rcPrevIsec);

    #pragma unroll
    for (int bounce = 0; bounce < MaxTracingDepth; bounce++) {
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
        bool isThisVertexConnectible = isBSDFConnectible(mat);

        if (rcPrevFound && !(isThisVertexConnectible || surf.isLight)) {
            IntersectionSetInvalid(rcData.rcPrevIsec);
            break;
        }

        if (surf.isLight) {
            break;
        }

        if (bounce == targetRcId - 1) {
            if (isThisVertexConnectible) {
                rcData.rcPrevIsec = isec;
                rcData.rcPrevWo = wo;
                rcData.rcPrevThroughput = throughput;
                rcPrevFound = true;
            }
            else {
                break;
            }
        }
        // make sure sample space is aligned for all paths
        vec4 lightRandSample = sample4f(rng);

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

vec3 retrace(uvec2 index, uvec2 frameSize) {
    const int MaxTracingDepth = 15;
    vec2 uv = (vec2(index) + 0.5) / vec2(frameSize);

    float depth;
    vec3 norm;
    vec3 albedo;
    int matMeshId;

    if (!unpackGBuffer(texture(uDepthNormal, uv), texelFetch(uAlbedoMatId, ivec2(index), 0), depth, norm, albedo, matMeshId)) {
        return vec3(0.0);
    }
    vec2 motion = texelFetch(uMotionVector, ivec2(index), 0).xy;
    int matId = matMeshId >> 16;

    Ray ray = pinholeCameraSampleRay(uCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));

    SurfaceInfo surf;
    surf.pos = ray.ori + ray.dir * (depth - 1e-4);
    surf.norm = norm;
    surf.albedo = albedo;
    surf.isLight = false;
    surf.matIndex = matId;

    // special case for primary hit - I can't make it to get instance and triangle id for first hit
    // so I recorded screen uv in isec.bary
    Intersection isec;
    isec.instanceIdx = SpecialHitIndex;
    isec.bary = uv;

    GRISReservoir resv = uGRISReservoir[index1D(index)];
    GRISReservoir temporalResv;

    if (!findPreviousReservoir(uv + motion, surf.pos, depth, norm, albedo, matMeshId, temporalResv)) {
        return vec3(0.0);
    }
    GRISPathSample temporalSample = temporalResv.pathSample;

    if (!GRISPathSampleIsValid(temporalSample)) {
        return vec3(0.0);
    }

    GRISReconnectionData rcData;

    traceReplayPathForHybridShift(isec, surf, ray, GRISPathFlagsRcVertexId(temporalSample.flags), temporalSample.primaryRng, rcData);
    uGRISReconnectionData[index1D(index)] = rcData;

    if (!IntersectionIsValid(rcData.rcPrevIsec)) {
        return vec3(0.0);
    }
    //return colorWheel(float(rcPrevIsec.instanceIdx == SpecialHitIndex));
    return vec3(rcData.rcPrevIsec.bary, 1.0);
}

#endif