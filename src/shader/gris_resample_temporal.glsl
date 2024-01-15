#ifndef GRIS_RESAMPLE_TEMPORAL_GLSL
#define GRIS_RESAMPLE_TEMPORAL_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "math.glsl"
#include "gris_reservoir.glsl"
#include "gris_retrace.glsl"

bool findPreviousReservoir(vec2 uv, vec3 pos, float depth, vec3 normal, vec3 albedo, int matMeshId, out GRISReservoir resv, out SurfaceInfo surf) {
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

    if (matMeshIdPrev != matMeshId || dot(normalPrev, normal) < 0.95 || distance(pos, posPrev) > 0.5) {
        return false;
    }
    resv = uGRISReservoirPrev[index1D(uvec2(pixelId))];
    surf = SurfaceInfo(posPrev, normalPrev, albedoPrev, matMeshIdPrev >> 16, false);
    return true;
}

vec3 temporalReuse(uvec2 index, uvec2 frameSize) {
    vec2 uv = (vec2(index) + 0.5) / vec2(frameSize);

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

    GRISReservoir resv = uGRISReservoir[index1D(uvec2(index))];

    Intersection dstPrimaryIsec;
    dstPrimaryIsec.instanceIdx = SpecialHitIndex;
    dstPrimaryIsec.bary = uv;

    if (uSettings.temporalReuse) {
        GRISReservoir temporalResv;
        GRISReservoirReset(temporalResv);

        SurfaceInfo dstPrimarySurf = SurfaceInfo(pos, norm, albedo, matId, false);
        SurfaceInfo srcPrimarySurf;

        if (((uCamera.frameIndex & CameraClearFlag) == 0) && findPreviousReservoir(uv + motion, pos, depth, norm, albedo, matMeshId, temporalResv, srcPrimarySurf)) {
            if (GRISReservoirIsValid(temporalResv)) {
                GRISReservoirReuseAndMerge(resv, dstPrimarySurf, dstPrimaryIsec, ray, temporalResv, srcPrimarySurf, resvRng);
            }
        }
    }
    GRISReservoirCapSample(resv, 40);

    GRISReservoirResetIfInvalid(resv);
    uGRISReservoirTemp[index1D(uvec2(index))] = resv;

    return clampColor(radiance);
}

#endif