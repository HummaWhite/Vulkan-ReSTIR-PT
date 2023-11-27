#ifndef LIGHT_SAMPLING_GLSL
#define LIGHT_SAMPLING_GLSL

vec3 sampleTriangleLight(TriangleLight light, vec3 ref, out vec3 wi, out float dist, out float pdf, vec2 r) {
    vec3 pos = sampleTriangleUniform(light.v0, light.v1, light.v2, r);
    dist = distance(ref, pos);

    vec3 n = vec3(light.nx, light.ny, light.nz);
    wi = (pos - ref) / dist;
    pdf = dist * dist / absDot(n, wi) / light.area;

    return light.radiance;
}

vec3 sampleLightByPower(vec3 ref, out vec3 wi, out float dist, out float pdf, vec4 r) {
    float sumPower = uLightSampleTable[0].prob;
    uint numLights = uLightSampleTable[0].failId;

    uint idx = uint(float(numLights) * r.x);
    idx = (r.y < uLightSampleTable[idx + 1].prob) ? idx : uLightSampleTable[idx + 1].failId - 1;

    TriangleLight light = uTriangleLights[idx];
    vec3 radiance = sampleTriangleLight(light, ref, wi, dist, pdf, r.zw);
    pdf *= luminance(light.radiance) * light.area / sumPower;

    return radiance;
}

vec3 sampleLightUniform(vec3 ref, out vec3 wi, out float dist, out float pdf, vec4 r) {
    uint numLights = uLightSampleTable[0].failId;
    uint idx = uint(float(numLights) * r.x);

    TriangleLight light = uTriangleLights[idx];
    vec3 radiance = sampleTriangleLight(light, ref, wi, dist, pdf, r.zw);
    pdf *= 1.0 / float(numLights);

    return radiance;
}

vec3 sampleLight(vec3 ref, out vec3 wi, out float dist, out float pdf, inout uint rng) {
    //return sampleLightByPower(ref, wi, dist, pdf, sample4f(rng));
    return sampleLightUniform(ref, wi, dist, pdf, sample4f(rng));
}

vec3 sampleLight(vec3 ref, out vec3 wi, out float dist, out float pdf, vec4 r) {
    //return sampleLightByPower(ref, wi, dist, pdf, r);
    return sampleLightUniform(ref, wi, dist, pdf, r);
}

vec3 sampleLightThreaded(vec3 ref, float blockRand, out vec3 wi, out float dist, out float pdf, inout uint rng) {
    float sumPower = uLightSampleTable[0].prob;
    uint numLights = uLightSampleTable[0].failId;

    const uint blockSize = 16384;
    uint blockNum = ceilDiv(numLights, blockSize);
    uint blockIdx = uint(blockRand * float(blockNum));

    uint realSize = (blockIdx == blockNum - 1) ? numLights - blockIdx * blockSize : blockSize;
    uint idx = blockSize * blockIdx + uint(sample1f(rng) * realSize);

    TriangleLight light = uTriangleLights[idx];
    vec3 radiance = sampleTriangleLight(light, ref, wi, dist, pdf, sample2f(rng));
    pdf *= 1.0 / float(numLights);

    return radiance;
}

#endif