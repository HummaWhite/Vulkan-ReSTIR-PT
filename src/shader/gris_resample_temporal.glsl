#ifndef GRIS_RESAMPLE_TEMPORAL_GLSL
#define GRIS_RESAMPLE_TEMPORAL_GLSL

#include "camera.glsl"
#include "ray_gbuffer_util.glsl"
#include "light_sampling.glsl"
#include "math.glsl"
#include "gris_reservoir.glsl"

vec3 temporalReuse(uvec2 index, uvec2 frameSize) {
    const int MaxTracingDepth = 15;
    vec2 uv = (vec2(index) + 0.5) / vec2(frameSize);

    GRISReconnectionData rcData = uGRISReconnectionData[index1D(index)];

    if (!intersectionIsValid(rcData.rcPrevIsec)) {
        return vec3(0.0);
    }

    SurfaceInfo rcSurf, rcPrevSurf;

    if (intersectionIsSpecial(rcData.rcPrevIsec)) {
        loadSurfaceInfo(rcData.rcPrevIsec.bary, frameSize, rcPrevSurf);
    }
    else {
        loadSurfaceInfo(rcData.rcPrevIsec, rcPrevSurf);
    }

    vec2 motion = texelFetch(uMotionVector, ivec2(index), 0).xy;
    ivec2 prevIndex = ivec2((uv + motion) * vec2(frameSize));

    if (prevIndex.x < 0 || prevIndex.x >= frameSize.x || prevIndex.y < 0 || prevIndex.y >= frameSize.y) {
        return vec3(0.0);
    }
    GRISReservoir temporalResv = uGRISReservoirPrev[index1D(prevIndex)];

    GRISPathSample rcSample = temporalResv.pathSample;

    if (!GRISPathSampleIsValid(rcSample)) {
        return vec3(0.0);
    }
    loadSurfaceInfo(rcSample.rcIsec, rcSurf);

    const uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;

    if (!traceVisibility(uTLAS, shadowRayFlags, 0xff, rcPrevSurf.pos, rcSurf.pos)) {
        return vec3(0.0);
    }
    Material rcMat = uMaterials[rcSurf.matIndex];
    Material rcPrevMat = uMaterials[rcPrevSurf.matIndex];

    vec3 L = vec3(0.0);
    vec3 rcPrevWi = normalize(rcSurf.pos - rcPrevSurf.pos);
    float distToPrev = length(rcSurf.pos - rcPrevSurf.pos);

    float jacobian = absDot(rcSurf.pos, rcPrevWi) / square(distToPrev) / rcSample.rcJacobian;

    //return clampColor(vec3(1.0 / jacobian));
    //return rcSample.rcLi;
    //return colorWheel(rcSample.rcPrevSamplePdf);

    uint rcType = GRISPathFlagsRcVertexType(rcSample.flags);

    if (distToPrev > GRISDistanceThreshold) {
        if (rcType == RcVertexTypeLightSampled || rcType == RcVertexTypeLightScattered) {
            L = rcSample.rcLi;
        }
        else {
            if (length(rcSample.rcWi) > 0.5) {
                L += rcSample.rcLi * evalBSDF(rcMat, rcSurf.albedo, rcSurf.norm, -rcPrevWi, rcSample.rcWi) * satDot(rcSurf.norm, rcSample.rcWi);
            }
            if (length(rcSample.rcWs) > 0.5) {
                L += rcSample.rcLs * evalBSDF(rcMat, rcSurf.albedo, rcSurf.norm, -rcPrevWi, rcSample.rcWs) * satDot(rcSurf.norm, rcSample.rcWs);
            }
        }
        L *= evalBSDF(rcPrevMat, rcPrevSurf.albedo, rcPrevSurf.norm, rcData.rcPrevWo, rcPrevWi) * satDot(rcPrevSurf.norm, rcPrevWi);
        L *= rcData.rcPrevThroughput;
        L /= rcSample.rcPrevSamplePdf;

        if (!isBlack(L)) {
            L = L / luminance(L) * temporalResv.resampleWeight;
        }
        else {
            L = vec3(0.0);
        }
    }
    return clampColor(L);
}

#endif