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
    uint mode;
    uint writeIndex;
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

vec3 traceReplayPathForHybridShift(SurfaceInfo surf, Ray ray, uint targetPathFlags, uint rng, out Intersection rcPrevIsec, out vec3 rcPrevWo) {
    const int MaxTracingDepth = 15;

    vec3 throughput = vec3(1.0);
    vec3 rcPrevThroughput = vec3(0.0);
    vec3 wo = -ray.dir;
    bool rcPrevFound = false;

    /*
    Material mat = uMaterials[surf.matIndex];
    BSDFSample s;
    Intersection isec;

    IntersectionSetInvalid(rcPrevIsec);

    uint targetRcVertexId = GRISPathFlagsRcVertexId(targetPathFlags);
    uint targetRcVertexType = GRISPathFlagsRcVertexType(targetPathFlags);

    #pragma unroll
    for (int bounce = 0; bounce < MaxTracingDepth; bounce++) {
        if (bounce == targetRcVertexId) {
            break;
        }

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

        if (rcPrevFound) {
            if (surf.isLight && (targetRcVertexType != RcVertexSource)) {
                // if target type is emitter source but actually not hitting an emitter at depth == targetRcVertexId
                //   not invertible
                IntersectionSetInvalid(rcPrevIsec);
            }
            break;
        }
        rcPrevIsec = isec;
        rcPrevWo = wo;
        rcPrevThroughput = throughput;

        bool isThisVertexConnectible = isBSDFConnectible(mat);

        if (!rcPrevFound && isThisVertexConnectible && (bounce == targetRcVertexId - 1)) {
            rcPrevFound = true;
        }

        // make sure sample space is aligned for all paths
        vec4 lightRandSample = sample4f(rng);

        if (rcPrevFound && (targetRcVertexType != RcVertexSource)) {
            bool isLightSamplable = bounce > 0 && !isBSDFDelta(mat);

            const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;

            vec3 lightRadiance, lightDir;
            float lightDist, lightPdf;

            lightRadiance = sampleLight(surf.pos, lightDir, lightDist, lightPdf, lightRandSample);

            bool shadowed = traceShadow(
                uTLAS,
                shadowRayFlags, 0xff,
                surf.pos, MinRayDistance, lightDir, lightDist - MinRayDistance
            );
            
            if ((!shadowed && lightPdf > 1e-6) && targetRcVertexType != RcVertexDirectLight) {
                IntersectionSetInvalid(rcPrevIsec);
            } else if (targetRcVertexType != RcVertexScattered) {
                IntersectionSetInvalid(rcPrevIsec);
            }
            break;
        }

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
    */
    return rcPrevThroughput;
}

#endif