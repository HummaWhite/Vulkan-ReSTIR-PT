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

#endif