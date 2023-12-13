#ifndef GI_NAIVE_GLSL
#define GI_NAIVE_GLSL

#include "camera.glsl"
#include "gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "material.glsl"

vec3 indirectIllumination(uvec2 index, uvec2 frameSize) {
    const int MaxTracingDepth = 15;
    vec2 uv = (vec2(index) + 0.5) / vec2(frameSize);

    vec4 depthNormal = texture(uDepthNormal, uv);
    float depth = depthNormal.x;
    vec3 norm = depthNormal.yzw;

    if (depth == 0.0) {
        return vec3(0.0);
    }
    uvec2 albedoMatId = texelFetch(uAlbedoMatId, ivec2(index), 0).rg;
    vec3 albedo = unpackAlbedo(albedoMatId.x);
    int matMeshId = int(albedoMatId.y);
    int matId = matMeshId >> 16;
    
    Ray ray = pinholeCameraSampleRay(uCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));
    uint rng = makeSeed(uCamera.seed + index.x, index.y);

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

        if (surf.isLight) {
            float cosTheta = -dot(ray.dir, surf.norm);

             if (/* cosTheta > 0 */ true) {
                float weight = 1.0;

                if (bounce > 0 && !isSampleTypeDelta(s.type)) {
                    float dist = length(surf.pos - lastPos);
                    float sumPower = uLightSampleTable[0].prob;
                    float lightPdf = luminance(surf.albedo) / sumPower * dist * dist / abs(cosTheta);
                    weight = MISWeight(s.pdf, lightPdf);
                }
                radiance += surf.albedo * weight * throughput;
            }
            break;
        }

        if (/* sample direct lighting */ bounce > 0 && !isBSDFDelta(mat)) {
            const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

            vec3 lightRadiance, lightDir;
            float lightDist, lightPdf;

            lightRadiance = sampleLight(surf.pos, lightDir, lightDist, lightPdf, rng);

            bool shadowed = traceShadow(
                uTLAS,
                shadowRayFlags, 0xff,
                surf.pos, MinRayDistance, lightDir, lightDist - MinRayDistance
            );

            if (!shadowed && lightPdf > 1e-6) {
                float bsdfPdf = absDot(surf.norm, lightDir) * PiInv;
                float weight = MISWeight(lightPdf, bsdfPdf);
                radiance += lightRadiance * evalBSDF(mat, surf.albedo, surf.norm, wo, lightDir) * satDot(surf.norm, lightDir) / lightPdf * weight * throughput;
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
    return clampColor(radiance);
}

#endif