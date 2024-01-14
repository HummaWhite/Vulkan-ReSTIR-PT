#ifndef DI_SPATIAL_GLSL
#define DI_SPATIAL_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "di_reservoir.glsl"

bool findNeighborReservoir(vec2 uv, vec3 pos, float depth, vec3 normal, vec3 albedo, int matMeshId, out DIReservoir resv, out SurfaceInfo surf) {
    if (uv.x < 0 || uv.y < 0 || uv.x > 1.0 || uv.y > 1.0) {
        return false;
    }
    ivec2 pixelId = ivec2(uv * vec2(uCamera.filmSize.xy));

    float depthPrev;
    vec3 normalPrev;
    vec3 albedoPrev;
    int matMeshIdPrev;

    if (!unpackGBuffer(texture(uDepthNormal, uv), texelFetch(uAlbedoMatId, pixelId, 0), depthPrev, normalPrev, albedoPrev, matMeshIdPrev)) {
        return false;
    }
    Ray ray = pinholeCameraSampleRay(uCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));
    vec3 posPrev = ray.ori + ray.dir * (depthPrev - 1e-4);

    if (dot(normalPrev, normal) < 0.8 || distance(pos, posPrev) > 1) {
        return false;
    }
    resv = uDIReservoirTemp[index1D(uvec2(pixelId))];
    surf = SurfaceInfo(posPrev, normalPrev, albedoPrev, matMeshIdPrev >> 16, false);
    return true;
}

vec3 spatialReuse(uvec2 index, uvec2 filmSize) {
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
    uint rng = makeSeed(uCamera.seed, index) ^ 2;
    uint initRng = rng;

    vec3 pos = ray.ori + ray.dir * (depth - 1e-4);
    vec3 wo = -ray.dir;
    Material mat = uMaterials[matId];

    vec3 radiance = vec3(0.0);

    DIReservoir resv = uDIReservoirTemp[index1D(uvec2(index))];

    if (uSettings.spatialReuse) {
        const uint ResampleNum = 3;
        const float ResampleRadius = 20.0;
        vec2 texelSize = 1.0 / vec2(filmSize);

        SurfaceInfo dstSurf = SurfaceInfo(pos, norm, albedo, matId, false);
        SurfaceInfo srcSurf;

#pragma unroll
        for (uint i = 0; i < ResampleNum; i++) {
            vec2 neighbor = uv + toConcentricDisk(sample2f(rng)) * ResampleRadius * texelSize;

            DIReservoir neighborResv;
            DIReservoirReset(neighborResv);

            if (findNeighborReservoir(neighbor, pos, depth, norm, albedo, matMeshId, neighborResv, srcSurf)) {
                if (DIReservoirIsValid(neighborResv)) {
                    DIReservoirReuseAndMerge(resv, dstSurf, neighborResv, srcSurf, wo, rng);
                }
            }
        }

        if (DIReservoirIsValid(resv) && DIPathSampleIsValid(resv.pathSample)) {
            SurfaceInfo surf;
            loadSurfaceInfo(resv.pathSample.isec, surf);

            const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;

            if (!traceVisibility(uTLAS, shadowRayFlags, 0xff, pos, surf.pos)) {
                resv.resampleWeight = 0;
            }
        }
    }
    DIReservoirCapSample(resv, 40);
    DIReservoirResetIfInvalid(resv);

    uDIReservoir[index1D(uvec2(index))] = resv;

    if (DIReservoirIsValid(resv) && DIPathSampleIsValid(resv.pathSample)) {
        DIPathSample pathSample = resv.pathSample;

        SurfaceInfo surf;
        loadSurfaceInfo(pathSample.isec, surf);

        vec3 wi = normalize(surf.pos - pos);

        if (resv.sampleCount > 0) {
            vec3 Li = pathSample.Li * evalBSDF(mat, albedo, norm, wo, wi) * satDot(norm, wi) / pathSample.samplePdf;

            if (!isBlack(Li)) {
                radiance = Li / luminance(Li) * resv.resampleWeight / float(resv.sampleCount);
            }
        }
    }
    //radiance = colorWheel(float(resv.pathSample.rng) / float(0xffffffffu));
    return clampColor(radiance);
}

#endif