#ifndef DI_PATH_GEN_GLSL
#define DI_PATH_GEN_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "di_reservoir.glsl"

vec3 generatePath(uvec2 index, uvec2 frameSize) {
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
    uint resvRng = ~rng;

    vec3 pos = ray.ori + ray.dir * (depth - 1e-4);
    vec3 wo = -ray.dir;
    Material mat = uMaterials[matId];
    SurfaceInfo surf = SurfaceInfo(pos, norm, albedo, matId, false);

    DIReservoir resv;
    DIReservoirReset(resv);

    DIPathSample pathSample;
    DIPathSampleInit(pathSample);

    vec3 radiance = sampleLi(surf, mat, wo, rng, resvRng, resv);
    
    uDIReservoir[index1D(uvec2(index))] = resv;

    pathSample = resv.pathSample;

    return assert(!pathSample.isLightSample && DIPathSampleIsValid(pathSample));

    radiance = vec3(0.0);

    if (DIReservoirIsValid(resv)) {

        SurfaceInfo surf;
        loadSurfaceInfo(pathSample.isec, surf);

        vec3 wi = normalize(surf.pos - pos);

        if (DIReservoirIsValid(resv) && resv.sampleCount > 0) {
            vec3 Li = pathSample.Li * evalBSDF(mat, albedo, norm, wo, wi) * satDot(norm, wi) / pathSample.samplePdf;

            if (!isBlack(Li)) {
                radiance = Li;
            }
        }
    }
    return clampColor(radiance);
}

#endif