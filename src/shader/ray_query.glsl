#ifndef RAY_QUERY_GLSL
#define RAY_QUERY_GLSL

#include "ray_layouts.glsl"

bool rayQueryTraceShadow(uint rayFlags, uint rayMask, vec3 origin, float minDist, vec3 direction, float maxDist) {
    rayQueryEXT rayQuery;

    rayQueryInitializeEXT(
        /* AS, flags, masks */ rayQuery, uTLAS, rayFlags, rayMask,
        /* ray parameters   */ origin, minDist, direction, maxDist
    );

    while (rayQueryProceedEXT(rayQuery)) {
        if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            // Reserved for alpha test
        }
    }
    return (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT);
}

Intersection rayQueryTrace(uint rayFlags, uint rayMask, vec3 origin, float minDist, vec3 direction, float maxDist) {
    Intersection isec;
    rayQueryEXT rayQuery;

    rayQueryInitializeEXT(
        /* AS, flags, masks */ rayQuery, uTLAS, rayFlags, rayMask,
        /* ray parameters   */ origin, minDist, direction, maxDist
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
        isec.dist = rayQueryGetIntersectionTEXT(rayQuery, true);
        isec.hit = true;
    }
    else {
        isec.hit = false;
    }
    return isec;
}

#endif