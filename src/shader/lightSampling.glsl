#ifndef LIGHT_SAMPLING_GLSL
#define LIGHT_SAMPLING_GLSL

vec3 sampleLight(ObjectInstance obj, vec3 ref, out float dist, out vec3 norm, vec3 r) {
    uint primCount = obj.indexCount / 3;
    uint primIdx = uint(float(primCount) * r.x);

    uint i0 = uIndices[obj.indexOffset + primIdx * 3 + 0];
    uint i1 = uIndices[obj.indexOffset + primIdx * 3 + 1];
    uint i2 = uIndices[obj.indexOffset + primIdx * 3 + 2];

    vec3 v0 = uVertices[i0].pos;
    vec3 v1 = uVertices[i1].pos;
    vec3 v2 = uVertices[i2].pos;

    vec3 pos = sampleTriangleUniform(v0, v1, v2, r.yz);
    pos = (obj.transform * vec4(pos, 1.0)).xyz;
    norm = normalize(cross(v1 - v0, v2 - v0));
    norm = (obj.transformInvT * vec4(norm, 0.0)).xyz;
    dist = distance(ref, pos);

    return (pos - ref) / dist;
}

vec3 sampleLight(vec3 ref, out vec3 dir, out float dist, out float pdf, inout uint rng) {
    float sumPower = uLightSampleTable[0].prob;
    uint numLights = uLightSampleTable[0].failId;

    vec2 r = sample2f(rng);
    uint idx = uint(float(numLights) * r.x);
    idx = (r.y < uLightSampleTable[idx + 1].prob) ? idx : uLightSampleTable[idx + 1].failId - 1;

    vec3 radiance = uLightInstances[idx].radiance;
    ObjectInstance obj = uObjectInstances[uLightInstances[idx].objectIdx];

    vec3 norm;
    dir = sampleLight(obj, ref, dist, norm, sample3f(rng));

    float surfaceArea = obj.transformedSurfaceArea;
    float pdfSelectLight = luminance(radiance * surfaceArea) / sumPower;
    float pdfUniformPoint = 1.0 / surfaceArea;
    float cosTheta = dot(norm, dir);

    if (cosTheta <= 0.0) {
        pdf = 0.0;
        return Black;
    }
    else {
        pdf = luminance(radiance) / sumPower * dist * dist / absDot(norm, dir);
        return radiance;
    }
}

#endif