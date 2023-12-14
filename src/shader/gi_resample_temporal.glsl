#ifndef GI_RESAMPLE_TEMPORAL_GLSL
#define GI_RESAMPLE_TEMPORAL_GLSL

#include "camera.glsl"
#include "gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "gi_reservoir.glsl"

bool findPreviousReservoir(vec2 uv, vec3 pos, float depth, vec3 normal, vec3 albedo, int matMeshId, out GIReservoir resv) {
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

    if (matMeshIdPrev != matMeshId || dot(normalPrev, normal) < 0.9 || abs(depthPrev - depth) > 5.0) {
        return false;
    }
    resv = uGIReservoirPrev[index1D(uvec2(pixelId))];
    return true;
}

vec3 indirectIllumination(uvec2 index, uvec2 frameSize) {
    const int MaxTracingDepth = 15;
    vec2 uv = (vec2(index) + 0.5) / vec2(frameSize.xy);

    float depth;
    vec3 norm;
    vec3 albedo;
    int matMeshId;

    if (!unpackGBuffer(texture(uDepthNormal, uv), texelFetch(uAlbedoMatId, ivec2(index), 0), depth, norm, albedo, matMeshId)) {
        GIReservoirReset(uGIReservoir[index1D(index)]);
        return vec3(0.0);
    }
    vec2 motion = texelFetch(uMotionVector, ivec2(index), 0).xy;
    int matId = matMeshId >> 16;
    
    Ray ray = pinholeCameraSampleRay(uCamera, vec2(uv.x, 1.0 - uv.y), vec2(0));
    uint rng = makeSeed(uCamera.seed + index.x, index.y);

    vec3 throughputFirst;
    vec3 throughputAfter = vec3(1.0);
    vec3 lastPos;
    vec3 wo = -ray.dir;

    SurfaceInfo surf;
    surf.pos = ray.ori + ray.dir * (depth - 1e-4);
    surf.norm = norm;
    surf.albedo = albedo;
    surf.isLight = false;

    GIPathSample pathSample = GIPathSampleInit();
    pathSample.visiblePos = surf.pos;
    pathSample.visibleNorm = surf.norm;
    
    vec3 primaryPos = surf.pos;
    vec3 primaryWo = -ray.dir;
    vec3 primaryAlbedo = albedo;
    float primaryPdf;

    Material mat = uMaterials[matId];
    BSDFSample s;
    Intersection isec;

    #pragma unroll
    for (int bounce = 0; bounce < MaxTracingDepth; bounce++) {
        if (bounce > 0) {
            isec = traceClosestHit(
                uTLAS,
                gl_RayFlagsOpaqueEXT, 0xff,
                ray.ori, MinRayDistance, ray.dir, MaxRayDistance
            );

            if (!IntersectionIsValid(isec)) {
                break;
            }
            loadSurfaceInfo(isec, surf);
            mat = uMaterials[surf.matIndex];

            if (bounce == 1) {
                pathSample.sampledPos = surf.pos;
                pathSample.sampledNorm = surf.norm;
            }
        }

        if (surf.isLight) {
            float cosTheta = -dot(ray.dir, surf.norm);

            if (/* cosTheta > 0 */ true) {
                float weight = 1.0;

                if (bounce > 0 && !isSampleTypeDelta(s.type)) {
                    float dist = length(surf.pos - lastPos);
                    float sumPower = uLightSampleTable[0].prob;
                    float lightPdf = luminance(surf.albedo) / sumPower * dist * dist / abs(cosTheta);
                    weight = MISWeight(s.pdf, lightPdf);
                }
                vec3 addition = surf.albedo * weight;
                pathSample.radiance += addition * (bounce == 1 ? vec3(1.0) : throughputAfter);
            }
            break;
        }

        if (/* sample direct lighting */ bounce > 0 && !isBSDFDelta(mat)) {
            const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

            vec3 lightRadiance, lightDir;
            float lightDist, lightPdf;

            lightRadiance = sampleLight(surf.pos, lightDir, lightDist, lightPdf, rng);

            bool shadowed = traceShadow(
                uTLAS,
                shadowRayFlags, 0xff,
                surf.pos, MinRayDistance, lightDir, lightDist - MinRayDistance
            );

            if (!shadowed && lightPdf > 1e-6) {
                float bsdfPdf = absDot(surf.norm, lightDir) * PiInv;
                float weight = MISWeight(lightPdf, bsdfPdf);
                pathSample.radiance += lightRadiance * evalBSDF(mat, surf.albedo, surf.norm, wo, lightDir) * satDot(surf.norm, lightDir) / lightPdf * weight * throughputAfter;
            }
        }
        if (/* russian roulette */ bounce > 4) {
            float pdfTerminate = max(1.0 - luminance(throughputAfter), 0);

            if (sample1f(rng) < pdfTerminate) {
                break;
            }
            throughputAfter /= (1.0 - pdfTerminate);
        }

        if (!sampleBSDF(mat, (bounce == 0 && !isBSDFDelta(mat)) ? vec3(1.0) : surf.albedo, surf.norm, wo, sample3f(rng), s) || s.pdf < 1e-6) {
            break;
        }
        if (bounce == 0) {
            primaryPdf = s.pdf;
        }
        float cosTheta = isSampleTypeDelta(s.type) ? 1.0 : absDot(surf.norm, s.wi);
        vec3 scatterTerms = s.bsdf * cosTheta / s.pdf;

        if (bounce == 0) {
            throughputFirst = scatterTerms;
        }
        else {
            throughputAfter *= scatterTerms;
        }
        lastPos = surf.pos;

        wo = -s.wi;
        ray.dir = s.wi;
        ray.ori = surf.pos + ray.dir * 1e-4;
    }
    vec3 radiance = pathSample.radiance * throughputFirst;

    GIReservoir resv;
    GIReservoirReset(resv);
    
    if (true) {
        if ((uCamera.frameIndex & CameraClearFlag) == 0) {
            findPreviousReservoir(uv + motion, primaryPos, depth, norm, albedo, matMeshId, resv);
        }
        float sampleWeight = 0.0;

        if (GIPathSampleIsValid(pathSample)) {
            float pHat = luminance(pathSample.radiance);
            sampleWeight = pHat / primaryPdf;

            if (isnan(sampleWeight) || sampleWeight < 0.0 || primaryPdf < 1e-6) {
                sampleWeight = 0.0;
            }
            GIReservoirAddSample(resv, pathSample, pHat, sampleWeight, sample1f(rng));
        }
        GIReservoirResetIfInvalid(resv);
        GIReservoirCapSample(resv, 40);

        if (GIReservoirIsValid(resv) && resv.sampleCount > 0 && resv.pHat > 1e-6 && isBSDFConnectible(uMaterials[matId])) {
            vec3 primaryWi = normalize(resv.sampledPos - primaryPos);
            radiance = resv.radiance / resv.pHat * resv.resampleWeight / float(resv.sampleCount);
            radiance *= evalBSDF(uMaterials[matId], albedo, norm, primaryWo, primaryWi) * satDot(norm, primaryWi);
        }
    }
    uGIReservoir[index1D(index)] = resv;
    return clampColor(radiance);
}

#endif