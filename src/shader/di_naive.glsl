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

        lightRadiance = sampleLight(pos, lightDir, lightDist, lightPdf, sample4f(rng));

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

struct StreamSampler {
    vec3 Li;
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

void addStream(inout StreamSampler resv, vec3 Li, float weight, float r) {
    resv.sumWeight += weight;

    if (r * resv.sumWeight < weight) {
        resv.weight = weight;
        resv.Li = Li;
        resv.sampleCount++;
    }
}

vec3 directIllumination2(uvec2 index, uvec2 frameSize) {
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

    StreamSampler resv = initStream();

    if (/* sample direct lighting */ !isBSDFDelta(mat)) {
        const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

        vec3 lightRadiance, lightDir;
        float lightDist, lightPdf;

        lightRadiance = sampleLight(pos, lightDir, lightDist, lightPdf, sample4f(rng));

        bool shadowed = traceShadow(
            uTLAS,
            shadowRayFlags, 0xff,
            pos, MinRayDistance, lightDir, lightDist - 1e-4
        );

        if (!shadowed && lightPdf > 1e-6) {
            float bsdfPdf = evalPdf(mat, norm, -ray.dir, lightDir);
            float weight = MISWeight(lightPdf, bsdfPdf);
            vec3 contrib = lightRadiance * evalBSDF(mat, albedo, norm, wo, lightDir) * satDot(norm, lightDir) / lightPdf * weight;

            addStream(resv, contrib, 100, sample1f(rng));
            radiance += contrib;
        }
    }
    BSDFSample s;

    if (sampleBSDF(mat, albedo, norm, wo, sample3f(rng), s) && s.pdf > 1e-6) {
        Intersection isec = traceClosestHit(
            uTLAS,
            gl_RayFlagsOpaqueEXT, 0xff,
            pos, MinRayDistance, s.wi, MaxRayDistance
        );

        if (intersectionIsValid(isec)) {
            SurfaceInfo surf;
            loadSurfaceInfo(isec, surf);
            float cosTheta = -dot(s.wi, surf.norm);

            if (surf.isLight
#if !SAMPLE_LIGHT_DOUBLE_SIDE
                && cosTheta > 0
#endif
                ) {
                float dist = length(surf.pos - pos);
                float sumPower = uLightSampleTable[0].prob;
                float lightPdf = luminance(surf.albedo) / sumPower * dist * dist / abs(cosTheta);
                float weight = MISWeight(s.pdf, lightPdf);

                vec3 wi = normalize(surf.pos - pos);
                vec3 contrib = surf.albedo * evalBSDF(mat, albedo, norm, wo, wi) * satDot(norm, wi) / s.pdf * weight;

                addStream(resv, contrib, 1, sample1f(rng));
                radiance += contrib;
            }
        }
    }

    if (resv.weight > 0 && resv.sumWeight > 0) {
        radiance = resv.Li * resv.sumWeight / resv.weight;
    }
    else {
        radiance = vec3(0.0);
    }

    return clampColor(radiance);
}

#endif