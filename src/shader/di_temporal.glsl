#ifndef DI_RESAMPLE_TEMPORAL_GLSL
#define DI_RESAMPLE_TEMPORAL_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "di_reservoir.glsl"

bool findPreviousReservoir(vec2 uv, vec3 pos, float depth, vec3 normal, vec3 albedo, int matMeshId, out DIReservoir resv, out SurfaceInfo surf) {
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

    if (matMeshIdPrev != matMeshId || dot(normalPrev, normal) < 0.95 || abs(depthPrev - depth) > 0.1 * depth) {
        return false;
    }
    resv = uDIReservoirPrev[index1D(uvec2(pixelId))];
    surf = SurfaceInfo(posPrev, normalPrev, albedoPrev, matMeshIdPrev >> 16, false);
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
    uint rng = makeSeed(uCamera.seed, index) ^ 1;
    uint resvRng = ~rng;

    vec3 pos = ray.ori + ray.dir * (depth - 1e-4);
    vec3 wo = -ray.dir;
    Material mat = uMaterials[matId];

    vec3 radiance = vec3(0.0);

    DIReservoir resv = uDIReservoir[index1D(uvec2(index))];

    if (uSettings.temporalReuse) {
        DIReservoir temporalResv;
        DIReservoirReset(temporalResv);

        SurfaceInfo srcSurf;
        SurfaceInfo dstSurf = SurfaceInfo(pos, norm, albedo, matId, false);

        if (((uCamera.frameIndex & CameraClearFlag) == 0) && findPreviousReservoir(uv + motion, pos, depth, norm, albedo, matMeshId, temporalResv, srcSurf)) {
            if (DIReservoirIsValid(temporalResv)) {
                DIReservoirReuseAndMerge(resv, dstSurf, temporalResv, srcSurf, wo, resvRng);
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
    uDIReservoirTemp[index1D(uvec2(index))] = resv;

    return clampColor(radiance);
}

#endif