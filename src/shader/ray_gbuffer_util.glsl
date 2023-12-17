#ifndef RAY_GBUFFER_UTIL_GLSL
#define RAY_GBUFFER_UTIL_GLSL

#include "gbuffer_util.glsl"

bool loadSurfaceInfo(vec2 uv, vec4 depthNormal, uvec4 albedoMatId, out SurfaceInfo surf) {
    float depth;
    vec3 norm;
    vec3 albedo;
    int matMeshId;

    if (unpackGBuffer(depthNormal, albedoMatId, depth, norm, albedo, matMeshId)) {
        if (depth == 0.0) {
            return false;
        }

        Ray ray = pinholeCameraSampleRay(uCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));

        surf.pos = ray.ori + ray.dir * (depth - 1e-4);
        surf.norm = norm;
        surf.albedo = albedo;
        surf.isLight = false;
        surf.matIndex = matMeshId >> 16;
        return true;
    }
    return false;
}

bool loadSurfaceInfo(vec2 uv, uvec2 frameSize, out SurfaceInfo surf) {
    uvec2 index = uvec2(uv * vec2(frameSize));
    return loadSurfaceInfo(uv, texture(uDepthNormal, uv), texelFetch(uAlbedoMatId, ivec2(index), 0), surf);
}

bool loadPrevSurfaceInfo(vec2 uv, uvec2 frameSize, out SurfaceInfo surf) {
    uvec2 index = uvec2(uv * vec2(frameSize));
    return loadSurfaceInfo(uv, texture(uDepthNormalPrev, uv), texelFetch(uAlbedoMatIdPrev, ivec2(index), 0), surf);
}

#endif