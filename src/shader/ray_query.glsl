#ifndef RAY_QUERY_GLSL
#define RAY_QUERY_GLSL

#include "ray_layouts.glsl"

bool rayQueryTraceShadow(
    accelerationStructureEXT topLevelAccelStructure,
    uint rayFlags, uint rayMask,
    vec3 origin, float minDist, vec3 direction, float maxDist
) {
    rayQueryEXT rayQuery;

    rayQueryInitializeEXT(
        /* rayQueryEXT    */ rayQuery,
        /* top level AS   */ topLevelAccelStructure,
        /* flags, masks   */ rayFlags, rayMask,
        /* ray parameters */ origin, minDist, direction, maxDist
    );

    while (rayQueryProceedEXT(rayQuery)) {
        if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            // Reserved for alpha test
        }
    }
    return (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT);
}

bool rayQueryTraceVisibility(
    accelerationStructureEXT topLevelAccelStructure,
    uint rayFlags, uint rayMask,
    vec3 from, vec3 to
) {
    return rayQueryTraceShadow(
        topLevelAccelStructure,
        rayFlags, rayMask,
        from, MinRayDistance, normalize(to - from), distance(to, from) - MinRayDistance
    );
}

Intersection rayQueryTraceClosestHit(
    accelerationStructureEXT topLevelAccelStructure,
    uint rayFlags, uint rayMask,
    vec3 origin, float minDist, vec3 direction, float maxDist
) {
    Intersection isec;
    rayQueryEXT rayQuery;

    rayQueryInitializeEXT(
        /* rayQueryEXT    */ rayQuery,
        /* top level AS   */ topLevelAccelStructure,
        /* flags, masks   */ rayFlags, rayMask,
        /* ray parameters */ origin, minDist, direction, maxDist
    );

    while (rayQueryProceedEXT(rayQuery)) {
        if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            // Reserved for alpha test
        }
    }
    
    if (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        isec.bary = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true).xy;
        isec.instanceIdx = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);
        isec.triangleIdx = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
    }
    else {
        isec.instanceIdx = InvalidHitIndex;
    }
    return isec;
}

float rayQueryDebugVisualizeAS(accelerationStructureEXT topLevelAccelStructure, vec3 origin, vec3 direction) {
    rayQueryEXT rayQuery;

    rayQueryInitializeEXT(
        /* rayQueryEXT    */ rayQuery,
        /* top level AS   */ topLevelAccelStructure,
        /* flags, masks   */ gl_RayFlagsNoneEXT, 0xff,
        /* ray parameters */ origin, MinRayDistance, direction, MaxRayDistance
    );

    float level = 0;

    while (rayQueryProceedEXT(rayQuery)) {
        if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            level += 1.0;
        }
    }
    if (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        //level = 100.0;
    }
    return level;
}

#endif