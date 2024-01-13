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

    if (matMeshIdPrev != matMeshId || dot(normalPrev, normal) < 0.99 || abs(depth - depthPrev) > 0.1 * depth) {
        return false;
    }
    resv = uDIReservoirTemp[index1D(uvec2(pixelId))];
    surf = SurfaceInfo(posPrev, normalPrev, albedoPrev, matMeshIdPrev >> 16, false);
    return true;
}

void randomReplay(inout DIReservoir dstResv, SurfaceInfo dstSurf, DIReservoir srcResv, SurfaceInfo srcSurf, vec3 wo, inout uint rng) {
    //DIReservoirMerge(dstResv, srcResv, sample1f(rng));

    Material dstMat = uMaterials[dstSurf.matIndex];
    float resampleWeight = 0;
    uint count = srcResv.sampleCount;
    count = 1;

    BSDFSample s;

    if (srcResv.pathSample.isLightSample && !isBSDFDelta(dstMat)) {
        const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

        vec3 lightRadiance, lightDir;
        vec2 lightBary;
        float lightDist, lightPdf, lightJacobian;
        uint lightId;

        lightRadiance = sampleLight(dstSurf.pos, lightDir, lightDist, lightPdf, lightJacobian, lightBary, lightId, sample4f(srcResv.pathSample.rng));

        bool shadowed = traceShadow(
            uTLAS,
            shadowRayFlags, 0xff,
            dstSurf.pos, MinRayDistance, lightDir, lightDist - MinRayDistance
        );

        if (!shadowed && lightPdf > 1e-6) {
            float bsdfPdf = evalPdf(dstMat, dstSurf.norm, wo, lightDir);
            float weight = MISWeight(lightPdf, bsdfPdf);

            if (uSettings.sampleMode == SampleModeLight) {
                weight = 1.0;
            }
            vec3 contrib = lightRadiance * evalBSDF(dstMat, dstSurf.albedo, dstSurf.norm, wo, lightDir) * satDot(dstSurf.norm, lightDir) / lightPdf * weight;
            resampleWeight = luminance(contrib) * float(count);
        }
    }
    else if (!srcResv.pathSample.isLightSample && sampleBSDF(dstMat, dstSurf.albedo, dstSurf.norm, wo, sample3f(srcResv.pathSample.rng), s) && s.pdf > 1e-6) {
        Intersection isec = traceClosestHit(
            uTLAS,
            gl_RayFlagsOpaqueEXT, 0xff,
            dstSurf.pos, MinRayDistance, s.wi, MaxRayDistance
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
                float dist = length(surf.pos - dstSurf.pos);
                float sumPower = uLightSampleTable[0].prob;
                float lightPdf = luminance(surf.albedo) / sumPower * dist * dist / abs(cosTheta);
                float weight = MISWeight(s.pdf, lightPdf);

                if (uSettings.sampleMode == SampleModeBSDF || isSampleTypeDelta(s.type)) {
                    weight = 1.0;
                }
                vec3 wi = normalize(surf.pos - dstSurf.pos);
                vec3 contrib = surf.albedo * evalBSDF(dstMat, dstSurf.albedo, dstSurf.norm, wo, wi) * satDot(dstSurf.norm, wi) / s.pdf * weight;
                resampleWeight = luminance(contrib) * float(count);
            }
        }
    }
    DIReservoirAddSample(dstResv, srcResv.pathSample, resampleWeight, count, sample1f(rng));
}

void reconnection(inout DIReservoir dstResv, SurfaceInfo dstSurf, DIReservoir srcResv, SurfaceInfo srcSurf, vec3 wo, inout uint rng) {
}

void reuseAndMerge(inout DIReservoir dstResv, SurfaceInfo dstSurf, DIReservoir srcResv, SurfaceInfo srcSurf, vec3 wo, inout uint rng) {
    if (uSettings.shiftMode == Reconnection) {
        reconnection(dstResv, dstSurf, srcResv, srcSurf, wo, rng);
    }
    else if (uSettings.shiftMode == Replay) {
        randomReplay(dstResv, dstSurf, srcResv, srcSurf, wo, rng);
    }
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
    uint rng = makeSeed(uCamera.seed + index.x, index.y) + 2;

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
                    reuseAndMerge(resv, dstSurf, neighborResv, srcSurf, wo, rng);
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
    return clampColor(radiance);
}

#endif