#ifndef LIGHT_SAMPLING_GLSL
#define LIGHT_SAMPLING_GLSL

#define SAMPLE_LIGHT_DOUBLE_SIDE 0

vec3 sampleTriangleLight(TriangleLight light, vec3 ref, out vec3 wi, out float dist, out float pdf, out float jacobian, out vec2 bary, vec2 r) {
    bary = uvToBary(r);

    vec3 pos = light.v0 * (1.0 - bary.x - bary.y) + light.v1 * bary.x + light.v2 * bary.y;
    dist = distance(ref, pos);

    vec3 n = vec3(light.nx, light.ny, light.nz);
    wi = (pos - ref) / dist;
    jacobian = absDot(n, wi) / square(dist);
    pdf = 1.0 / jacobian / light.area;

#if !SAMPLE_LIGHT_DOUBLE_SIDE
    if (dot(n, wi) > 0) {
        light.radiance = vec3(0.0);
    }
#endif
    return light.radiance;
}

vec3 sampleLightByPower(vec3 ref, out vec3 wi, out float dist, out float pdf, out float jacobian, out vec2 bary, out uint id, vec4 r) {
    float sumPower = uLightSampleTable[0].prob;
    uint numLights = uLightSampleTable[0].failId;

    id = uint(float(numLights) * r.x);
    id = (r.y < uLightSampleTable[id + 1].prob) ? id : uLightSampleTable[id + 1].failId - 1;

    TriangleLight light = uTriangleLights[id];
    vec3 radiance = sampleTriangleLight(light, ref, wi, dist, pdf, jacobian, bary, r.zw);
    pdf *= luminance(light.radiance) * light.area / sumPower;

    return radiance;
}

vec3 sampleLightUniform(vec3 ref, out vec3 wi, out float dist, out float pdf, out float jacobian, out vec2 bary, out uint id, vec4 r) {
    uint numLights = uLightSampleTable[0].failId;
    id = uint(float(numLights) * r.x);

    TriangleLight light = uTriangleLights[id];
    vec3 radiance = sampleTriangleLight(light, ref, wi, dist, pdf, jacobian, bary, r.zw);
    pdf *= 1.0 / float(numLights);

    return radiance;
}

vec3 sampleLight(vec3 ref, out vec3 wi, out float dist, out float pdf, out float jacobian, out vec2 bary, out uint id, vec4 r) {
    return sampleLightByPower(ref, wi, dist, pdf, jacobian, bary, id, r);
    //return sampleLightUniform(ref, wi, dist, pdf, jacobian, bary, id, r);
}

vec3 sampleLight(vec3 ref, out vec3 wi, out float dist, out float pdf, vec4 r) {
    vec3 dummy;
    uint id;
    return sampleLightByPower(ref, wi, dist, pdf, dummy.x, dummy.yz, id, r);
}

vec3 sampleLightThreaded(vec3 ref, float blockRand, out vec3 wi, out float dist, out float pdf, inout uint rng) {
    float sumPower = uLightSampleTable[0].prob;
    uint numLights = uLightSampleTable[0].failId;

    const uint blockSize = 16384;
    uint blockNum = ceilDiv(numLights, blockSize);
    uint blockIdx = uint(blockRand * float(blockNum));

    uint realSize = (blockIdx == blockNum - 1) ? numLights - blockIdx * blockSize : blockSize;
    uint idx = blockSize * blockIdx + uint(sample1f(rng) * realSize);
    vec3 dummy;

    TriangleLight light = uTriangleLights[idx];
    vec3 radiance = sampleTriangleLight(light, ref, wi, dist, pdf, dummy.x, dummy.yz, sample2f(rng));
    pdf *= 1.0 / float(numLights);

    return radiance;
}

#endif