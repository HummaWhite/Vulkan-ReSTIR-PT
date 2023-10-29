#ifndef LIGHT_SAMPLING_GLSL
#define LIGHT_SAMPLING_GLSL

vec3 sampleLight(ObjectInstance obj, vec3 ref, out float dist, out vec3 norm, out float pdf, vec3 r) {
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
    dist = distance(ref, pos);

    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;

    norm = normalize(cross(e1, e2));
    norm = mat3(obj.transformInvT) * norm;

    mat3 transform3x3 = mat3(obj.transform);
    pdf = 2.0 / length(cross(transform3x3 * e1, transform3x3 * e2)) / float(primCount);

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
    float pdfArea;
    dir = sampleLight(obj, ref, dist, norm, pdfArea, sample3f(rng));

    float cosTheta = dot(norm, dir);

    if (/* cosTheta <= 0.0 */ false) {
        pdf = 0.0;
        return Black;
    }
    else {
        pdf = pdfArea * dist * dist / absDot(norm, dir) * luminance(radiance * obj.transformedSurfaceArea) / sumPower;
        return radiance;
    }
}

#endif