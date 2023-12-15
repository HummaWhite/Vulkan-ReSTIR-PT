#ifndef RAY_PIPELINE_GLSL
#define RAY_PIPELINE_GLSL

#include "ray_layouts.glsl"

layout(location = ClosestHitPayloadLocation) rayPayloadEXT Intersection rtIsec;
layout(location = ShadowPayloadLocation) rayPayloadEXT bool rtShadowed;

bool traceShadow(
    accelerationStructureEXT topLevelAccelStructure,
    uint rayFlags, uint rayMask,
    vec3 origin, float minDist, vec3 direction, float maxDist
) {
    rtShadowed = true;

    traceRayEXT(
        /* AS, flags, masks    */ topLevelAccelStructure, rayFlags, rayMask,
        /* SBT offset & stride */ 0, 0,
        /* miss index          */ 1,
        /* ray attribs         */ origin, minDist, direction, maxDist,
        /* payload location    */ ShadowPayloadLocation
    );
    return rtShadowed;
}

bool traceVisibility(
    accelerationStructureEXT topLevelAccelStructure,
    uint rayFlags, uint rayMask,
    vec3 from, vec3 to
) {
    return !traceShadow(
        topLevelAccelStructure,
        rayFlags, rayMask,
        from, MinRayDistance, normalize(to - from), distance(to, from) - MinRayDistance
    );
}

Intersection traceClosestHit(
    accelerationStructureEXT topLevelAccelStructure,
    uint rayFlags, uint rayMask,
    vec3 origin, float minDist, vec3 direction, float maxDist
) {
    traceRayEXT(
        /* AS, flags, masks    */ topLevelAccelStructure, rayFlags, rayMask,
        /* SBT offset & stride */ 0, 0,
        /* miss index          */ 0,
        /* ray attribs         */ origin, minDist, direction, maxDist,
        /* payload location    */ ClosestHitPayloadLocation
    );
    return rtIsec;
}

#endif