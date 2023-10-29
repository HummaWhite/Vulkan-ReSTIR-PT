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

vec3 sampleLightGlobal(vec3 ref, out vec3 wi, out float dist, out float pdf, vec4 r) {
    float sumPower = uLightSampleTable[0].prob;
    uint numLights = uLightSampleTable[0].failId;

    uint idx = uint(float(numLights) * r.x);
    idx = (r.y < uLightSampleTable[idx + 1].prob) ? idx : uLightSampleTable[idx + 1].failId - 1;

    TriangleLight light = uTriangleLights[idx];

    vec3 pos = sampleTriangleUniform(light.v0, light.v1, light.v2, r.zw);
    dist = distance(ref, pos);

    vec3 n = vec3(light.nx, light.ny, light.nz);
    wi = (pos - ref) / dist;
    pdf = dist * dist / absDot(n, wi) * luminance(light.radiance) / sumPower;

    return light.radiance;
}

#endif