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
    uint rng = makeSeed(uCamera.seed + index.x, index.y);

    vec3 radiance = vec3(0.0);
    vec3 pos = ray.ori + ray.dir * (depth - 1e-4);
    vec3 wo = -ray.dir;
    Material mat = uMaterials[matId];

    DIReservoir resv;
    DIReservoirReset(resv);

    DIPathSample pathSample;
    DIPathSampleInit(pathSample);

    uint lightRng = rng;
    vec4 lightRandSample = sample4f(rng);
    uint scatterRng = rng;
    vec3 scatterRandSample = sample3f(rng);

    if ((uSettings.sampleMode != SampleModeBSDF) && !isBSDFDelta(mat)) {
        const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

        vec3 lightRadiance, lightDir;
        vec2 lightBary;
        float lightDist, lightPdf, lightJacobian;
        uint lightId;

        lightRadiance = sampleLight(pos, lightDir, lightDist, lightPdf, lightJacobian, lightBary, lightId, lightRandSample);

        bool shadowed = traceShadow(
            uTLAS,
            shadowRayFlags, 0xff,
            pos, MinRayDistance, lightDir, lightDist - MinRayDistance
        );

        if (!shadowed && lightPdf > 1e-6) {
            float bsdfPdf = evalPdf(mat, norm, wo, lightDir);
            float weight = MISWeight(lightPdf, bsdfPdf);

            if (uSettings.sampleMode == SampleModeLight) {
                weight = 1.0;
            }

            vec3 contrib = lightRadiance * evalBSDF(mat, albedo, norm, wo, lightDir) * satDot(norm, lightDir) / lightPdf * weight;

            pathSample.isec = Intersection(lightBary, 0, lightId);
            pathSample.Li = lightRadiance * weight;
            pathSample.rng = lightRng;
            pathSample.jacobian = lightJacobian;
            pathSample.samplePdf = lightPdf;
            pathSample.isLightSample = true;

            DIReservoirAddSample(resv, pathSample, luminance(contrib), sample1f(rng));
            radiance += contrib;
        }
    }
    BSDFSample s;

    DIPathSampleInit(pathSample);
    pathSample.isLightSample = false;

    if ((uSettings.sampleMode != SampleModeLight) && sampleBSDF(mat, albedo, norm, wo, scatterRandSample, s) && s.pdf > 1e-6) {
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

                if (uSettings.sampleMode == SampleModeBSDF || isSampleTypeDelta(s.type)) {
                    weight = 1.0;
                }
                float cosTerm = isSampleTypeDelta(s.type) ? 1.0 : satDot(norm, s.wi);

                vec3 wi = normalize(surf.pos - pos);
                vec3 contrib = surf.albedo * s.bsdf * cosTerm / s.pdf * weight;

                pathSample.isec = isec;
                pathSample.Li = surf.albedo * weight;
                pathSample.rng = scatterRng;
                pathSample.jacobian = abs(cosTheta) / square(dist);
                pathSample.samplePdf = s.pdf;
                pathSample.isLightSample = false;

                DIReservoirAddSample(resv, pathSample, luminance(contrib), sample1f(rng));
                radiance += contrib;
            }
        }
    }
    DIReservoirResetIfInvalid(resv);

    bool valid = false;

    if (resv.sampleCount > 0 && DIPathSampleIsValid(resv.pathSample) && resv.weight > 0) {
        resv.pathSample.Li *= resv.resampleWeight / resv.weight;
        resv.weight = resv.resampleWeight;
        valid = true;
    }
    else {
        DIPathSampleInit(resv.pathSample);
        resv.weight = 0;
        resv.resampleWeight = 0;
    }
    resv.sampleCount = 1;
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