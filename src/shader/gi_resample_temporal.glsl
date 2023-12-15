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

    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);
    vec3 rcThroughput;
    vec3 lastPos;
    vec3 wo = -ray.dir;

    SurfaceInfo surf;
    surf.pos = ray.ori + ray.dir * (depth - 1e-4);
    surf.norm = norm;
    surf.albedo = albedo;
    surf.isLight = false;

    GIPathSample pathSample = GIPathSampleInit();
    pathSample.rcPrevCoord = index.y << 16 | index.x;
    pathSample.rcLi = vec3(0.0);
    pathSample.rcWi = vec3(0.0);
    pathSample.rcLs = vec3(0.0);
    pathSample.rcWs = vec3(0.0);
    
    vec3 primaryPos = surf.pos;
    vec3 primaryWo = -ray.dir;

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
        }

        float cosPrevWi = dot(ray.dir, surf.norm);
        float distToPrev = distance(lastPos, surf.pos);
        float geometryJacobian = abs(cosPrevWi) / square(distToPrev);

        if (surf.isLight && bounce == 1) {
            pathSample.rcIsec.instanceIdx = InvalidHitIndex;
            break;
        }
        else if (surf.isLight && bounce > 1) {
            float cosTheta = -dot(ray.dir, surf.norm);

            if (/* cosTheta > 0 */ true) {
                float weight = 1.0;
                float sumPower = uLightSampleTable[0].prob;
                float lightPdf = luminance(surf.albedo) / sumPower / geometryJacobian;

                if (!isSampleTypeDelta(s.type)) {
                    weight = MISWeight(s.pdf, lightPdf);
                }
                vec3 weightedLi = surf.albedo * weight;

                if (bounce > 1) {
                    pathSample.rcLs += weightedLi * rcThroughput;
                }
                radiance += weightedLi * throughput;
            }
            break;
        }

        if (bounce == 1) {
            pathSample.rcIsec = isec;
            pathSample.rcJacobian = geometryJacobian;
            pathSample.rcPrevScatterPdf = s.pdf;
            pathSample.rcRng = rng;
        }
        vec4 lightRandSample = sample4f(rng);

        if (/* sample direct lighting */ bounce > 0 && !isBSDFDelta(mat)) {
            const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

            vec3 lightRadiance, lightDir;
            float lightDist, lightPdf;

            lightRadiance = sampleLight(surf.pos, lightDir, lightDist, lightPdf, lightRandSample);

            bool shadowed = traceShadow(
                uTLAS,
                shadowRayFlags, 0xff,
                surf.pos, MinRayDistance, lightDir, lightDist - MinRayDistance
            );

            if (!shadowed && lightPdf > 1e-6) {
                float bsdfPdf = absDot(surf.norm, lightDir) * PiInv;
                float weight = MISWeight(lightPdf, bsdfPdf);
                vec3 scatterTerm = evalBSDF(mat, surf.albedo, surf.norm, wo, lightDir) * satDot(surf.norm, lightDir);
                vec3 weightedLi = lightRadiance / lightPdf * weight;

                if (bounce == 1) {
                    pathSample.rcLi = weightedLi * rcThroughput;
                    pathSample.rcWi = lightDir;
                }
                else {
                    pathSample.rcLs += weightedLi * scatterTerm * rcThroughput;
                }
                radiance += weightedLi * scatterTerm * throughput;
            }
        }

        if (/* russian roulette */ bounce > 4) {
            float pdfTerminate = max(1.0 - luminance(throughput), 0);

            if (sample1f(rng) < pdfTerminate) {
                break;
            }
            throughput /= (1.0 - pdfTerminate);
            rcThroughput /= (1.0 - pdfTerminate);
        }

        if (!sampleBSDF(mat, (bounce == 0) ? surf.albedo : surf.albedo, surf.norm, wo, sample3f(rng), s) || s.pdf < 1e-6) {
            break;
        }
        float cosTheta = isSampleTypeDelta(s.type) ? 1.0 : absDot(surf.norm, s.wi);
        vec3 scatterTerm = s.bsdf * cosTheta / s.pdf;
        throughput *= scatterTerm;

        if (bounce == 0) {
            rcThroughput = vec3(1.0) / s.pdf;
        }
        else if (bounce == 1) {
            pathSample.rcWs = s.wi;
            rcThroughput /= s.pdf;
        }
        else if (bounce > 1) {
            rcThroughput *= scatterTerm;
        }

        lastPos = surf.pos;
        wo = -s.wi;
        ray.dir = s.wi;
        ray.ori = surf.pos + ray.dir * 1e-4;
    }

    GIReservoir resv;
    GIReservoirReset(resv);
    float weight = 1.0;
    
    if (true) {
        if ((uCamera.frameIndex & CameraClearFlag) == 0) {
            findPreviousReservoir(uv + motion, primaryPos, depth, norm, albedo, matMeshId, resv);
        }

        if (GIPathSampleIsValid(pathSample)) {
            float sampleWeight = luminance(radiance);

            if (isnan(sampleWeight) || sampleWeight < 0.0) {
                sampleWeight = 0.0;
            }
            GIReservoirAddSample(resv, pathSample, sampleWeight, sample1f(rng));
        }
        GIReservoirResetIfInvalid(resv);
        GIReservoirCapSample(resv, 40);

        if (GIReservoirIsValid(resv) && resv.sampleCount > 0) {
            pathSample = resv.pathSample;
            weight = resv.resampleWeight / float(resv.sampleCount);
        }
    }
    uGIReservoir[index1D(index)] = resv;
    Material primaryMat = uMaterials[matId];

    if (!isBSDFDelta(primaryMat)) {
        isec = pathSample.rcIsec;
        loadSurfaceInfo(isec, surf);
        Material rcMat = uMaterials[surf.matIndex];

        vec3 primaryWi = normalize(surf.pos - primaryPos);

        vec3 Li = vec3(0.0);

        if (length(pathSample.rcWi) > 0.5) {
            Li += pathSample.rcLi * evalBSDF(rcMat, surf.albedo, surf.norm, -primaryWi, pathSample.rcWi) * satDot(surf.norm, pathSample.rcWi);
        }
        if (length(pathSample.rcWs) > 0.5) {
            Li += pathSample.rcLs * evalBSDF(rcMat, surf.albedo, surf.norm, -primaryWi, pathSample.rcWs) * satDot(surf.norm, pathSample.rcWs);
        }
        Li = Li * evalBSDF(primaryMat, albedo, norm, primaryWo, primaryWi) * satDot(norm, primaryWi);

        if (!isBlack(Li) && traceVisibility(uTLAS, gl_RayFlagsNoneEXT, 0xff, primaryPos, surf.pos)) {
            radiance = Li / luminance(Li) * weight;
        }
    }
    return clampColor(radiance);
}

#endif