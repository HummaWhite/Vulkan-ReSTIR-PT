#ifndef DI_RESAMPLE_TEMPORAL_GLSL
#define DI_RESAMPLE_TEMPORAL_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "di_reservoir.glsl"

struct Settings {
    uint shiftMode;
    uint sampleMode;
    bool temporalReuse;
    bool spatialReuse;
};

layout(push_constant) uniform _Settings{
    Settings uSettings;
};

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

vec3 temporalReuse(uvec2 index, uvec2 filmSize) {
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

    vec3 radiance = vec3(0.0);

    DIReservoir resv = uDIReservoir[index1D(uvec2(index))];

    if (uSettings.temporalReuse) {
        DIReservoir temporalResv;

        if (((uCamera.frameIndex & CameraClearFlag) == 0) && findPreviousReservoir(uv + motion, pos, depth, norm, albedo, matMeshId, temporalResv)) {
            if (DIReservoirIsValid(temporalResv)) {
                DIReservoirMerge(resv, temporalResv, sample1f(rng));
                DIReservoirCapSample(resv, 40);
            }
        }
    }
    DIReservoirResetIfInvalid(resv);
    uDIReservoirTemp[index1D(uvec2(index))] = resv;

    /*
    if (DIReservoirIsValid(resv)) {
        DIPathSample pathSample = resv.pathSample;

        SurfaceInfo surf;
        loadSurfaceInfo(pathSample.isec, surf);

        vec3 wi = normalize(surf.pos - pos);

        if (DIReservoirIsValid(resv) && resv.sampleCount > 0) {
            vec3 Li = pathSample.Li * evalBSDF(mat, albedo, norm, wo, wi) * satDot(norm, wi) / pathSample.samplePdf;

            if (!isBlack(Li)) {
                radiance = Li / luminance(Li) * resv.resampleWeight / float(resv.sampleCount);
            }
        }
    }
    */
    return clampColor(radiance);
}

#endif