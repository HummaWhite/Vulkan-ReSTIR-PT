#ifndef DI_RESAMPLE_TEMPORAL_GLSL
#define DI_RESAMPLE_TEMPORAL_GLSL

#include "camera.glsl"
#include "gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "di_reservoir.glsl"

bool findPreviousReservoir(vec2 uv, vec3 pos, float depth, vec3 normal, vec3 albedo, int matMeshId, out DIReservoir resv) {
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

    if (matMeshIdPrev != matMeshId || dot(normalPrev, normal) < 0.95 || abs(depthPrev - depth) > 1.0) {
        return false;
    }
    resv = uDIReservoirPrev[index1D(uvec2(pixelId))];
    return true;
}

vec3 directIllumination(uvec2 index, uvec2 filmSize) {
    const uint ResampleNum = 32;

    vec2 uv = (vec2(index) + 0.5) / vec2(filmSize);

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
    uint rng = makeSeed(uCamera.seed + index.x, index.y);

    vec3 pos = ray.ori + ray.dir * (depth - 1e-4);
    vec3 wo = -ray.dir;
    Material mat = uMaterials[matId];

    DIReservoir resv;
    DIReservoirReset(resv);

    float blockRand = sample1f(rng);

    #pragma unroll
    for (uint i = 0; i < ResampleNum; i++) {
        DIPathSample pathSample = DIPathSampleInit();
        float lightPdf;

        //pathSample.radiance = sampleLight(pos, pathSample.wi, pathSample.dist, lightPdf, rng);
        pathSample.radiance = sampleLightThreaded(pos, blockRand, pathSample.wi, pathSample.dist, lightPdf, rng);
        float resampleWeight = 0;

        if (lightPdf > 1e-6) {
            float bsdfPdf = evalPdf(mat, norm, wo, pathSample.wi) * PiInv;
            float weight = MISWeight(lightPdf, bsdfPdf);
            weight = 1.0;
            vec3 pHat = pathSample.radiance * evalBSDF(mat, albedo, norm, wo, pathSample.wi) * satDot(norm, pathSample.wi) * weight;
            resampleWeight = DIToScalar(pHat / lightPdf);
        }

        if (isnan(resampleWeight)) {
            resampleWeight = 0;
        }
        DIReservoirAddSample(resv, pathSample, resampleWeight, sample1f(rng));
    }

    if (/* shadow ray */ true) {
        const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

        bool shadowed = traceShadow(
            uTLAS,
            shadowRayFlags, 0xff,
            pos, MinRayDistance, resv.wi, resv.dist - 1e-4
        );

        if (shadowed) {
            resv.resampleWeight = 0;
        }
    }

    DIReservoir temporalResv;

    if (((uCamera.frameIndex & CameraClearFlag) == 0) && findPreviousReservoir(uv + motion, pos, depth, norm, albedo, matMeshId, temporalResv)) {
        if (DIReservoirIsValid(temporalResv)) {
            DIReservoirMerge(resv, temporalResv, sample1f(rng));
            DIReservoirCapSample(resv, ResampleNum * 10);
        }
    }
    DIReservoirResetIfInvalid(resv);
    uDIReservoir[index1D(uvec2(index))] = resv;

    vec3 radiance = vec3(0.0);

    if (DIReservoirIsValid(resv) && resv.sampleCount > 0) {
        vec3 pHat = resv.radiance * evalBSDF(mat, albedo, norm, wo, resv.wi) * satDot(norm, resv.wi);
        radiance = pHat / luminance(pHat) * resv.resampleWeight / float(resv.sampleCount);
    }
    return clampColor(radiance);
}

#endif