#ifndef GI_NAIVE_GLSL
#define GI_NAIVE_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "material.glsl"

struct Resv {
    float resampleWeight;
    float weight;
    vec3 F;
};

void addResv(inout Resv resv, vec3 F, float weight, float r) {
    resv.resampleWeight += weight;

    if (r * resv.resampleWeight < weight) {
        resv.weight = weight;
        resv.F = F;
    }
}

Resv initResv() {
    return Resv(0, 0, vec3(0));
}

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
    uint rng = makeSeed(uCamera.seed, index);

    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);
    vec3 lastPos;
    vec3 wo = -ray.dir;

    SurfaceInfo surf;
    surf.pos = ray.ori + ray.dir * (depth - 1e-4);
    surf.norm = norm;
    surf.albedo = albedo;
    surf.isLight = false;

    Material mat = uMaterials[matId];
    BSDFSample s;
    Intersection isec;

    Resv resv = initResv();

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

        if (surf.isLight) {
            float cosTheta = -dot(ray.dir, surf.norm);

            if (bounce > 1
#if !SAMPLE_LIGHT_DOUBLE_SIDE
                && cosTheta > 0
#endif
                ) {
                float weight = 1.0;

                if (!isSampleTypeDelta(s.type)) {
                    float dist = length(surf.pos - lastPos);
                    float sumPower = uLightSampleTable[0].prob;
                    float lightPdf = luminance(surf.albedo) / sumPower * dist * dist / abs(cosTheta);
                    weight = MISWeight(s.pdf, lightPdf);
                }
                vec3 contrib = surf.albedo * weight * throughput;
                radiance += contrib;
                //addResv(resv, contrib, luminance(contrib), sample1f(rng));
            }
            break;
        }

        if (/* sample direct lighting */ bounce > 0 && !isBSDFDelta(mat)) {
            const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

            vec3 lightRadiance, lightDir;
            float lightDist, lightPdf;

            lightRadiance = sampleLight(surf.pos, lightDir, lightDist, lightPdf, sample4f(rng));

            bool shadowed = traceShadow(
                uTLAS,
                shadowRayFlags, 0xff,
                surf.pos, MinRayDistance, lightDir, lightDist - MinRayDistance
            );

            if (!shadowed && lightPdf > 1e-6) {
                float bsdfPdf = absDot(surf.norm, lightDir) * PiInv;
                float weight = MISWeight(lightPdf, bsdfPdf);
                weight = 1.0;

                vec3 contrib = lightRadiance * evalBSDF(mat, surf.albedo, surf.norm, wo, lightDir) * satDot(surf.norm, lightDir) / lightPdf * weight * throughput;
                radiance += contrib;
                //addResv(resv, contrib, luminance(contrib), sample1f(rng));
            }
        }
        if (/* russian roulette */ bounce > 4) {
            float pdfTerminate = max(1.0 - luminance(throughput), 0);

            if (sample1f(rng) < pdfTerminate) {
                break;
            }
            throughput /= (1.0 - pdfTerminate);
        }

        if (!sampleBSDF(mat, surf.albedo, surf.norm, wo, sample3f(rng), s) || s.pdf < 1e-6) {
            break;
        }

        float cosTheta = isSampleTypeDelta(s.type) ? 1.0 : absDot(surf.norm, s.wi);
        throughput *= s.bsdf * cosTheta / s.pdf;
        lastPos = surf.pos;
        
        wo = -s.wi;
        ray.dir = s.wi;
        ray.ori = surf.pos + ray.dir * 1e-4;
    }
    //radiance = resv.F * resv.resampleWeight / resv.weight;

    return clampColor(radiance);
}

#endif