#ifndef DI_NAIVE_GLSL
#define DI_NAIVE_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "material.glsl"

vec3 directIllumination(uvec2 index, uvec2 frameSize) {
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
    vec3 pos = ray.ori + ray.dir * (depth - 1e-4);
    vec3 wo = -ray.dir;
    Material mat = uMaterials[matId];

    if (/* sample direct lighting */ !isBSDFDelta(mat)) {
        const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

        vec3 lightRadiance, lightDir;
        float lightDist, lightPdf;

        lightRadiance = sampleLight(pos, lightDir, lightDist, lightPdf, rng);

        bool shadowed = traceShadow(
            uTLAS,
            shadowRayFlags, 0xff,
            pos, MinRayDistance, lightDir, lightDist - 1e-4
        );

        if (!shadowed && lightPdf > 1e-6) {
            float bsdfPdf = evalPdf(mat, norm, -ray.dir, lightDir);
            float weight = MISWeight(lightPdf, bsdfPdf);
            weight = 1.0;
            radiance += lightRadiance * evalBSDF(mat, albedo, norm, wo, lightDir) * satDot(norm, lightDir) / lightPdf * weight;
        }
    }
    return clampColor(radiance);
}

#endif