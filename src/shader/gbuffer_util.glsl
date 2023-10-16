#ifndef GBUFFER_UTIL_GLSL
#define GBUFFER_UTIL_GLSL

uint packAlbedo(vec3 albedo) {
    return packUnorm4x8(vec4(albedo, 1.0));
}

vec3 unpackAlbedo(uint packed) {
    return unpackUnorm4x8(packed).rgb;
}

uvec2 packNormal(vec3 n) {
    vec2 p = n.xy * (1.0 / (abs(n.x) + abs(n.y) + n.z));
    return floatBitsToUint(vec2(p.x + p.y, p.x - p.y));
}

vec3 unpackNormal(uvec2 packed) {
    vec2 p = uintBitsToFloat(packed);
    vec2 temp = vec2(p.x + p.y, p.x - p.y) * 0.5;
    vec3 v = vec3(temp, 1.0 - abs(temp.x) - abs(temp.y));
    return normalize(v);
}

uint packMotionVector(vec2 motion) {
    return packSnorm2x16(motion);
}

vec2 unpackMotionVector(uint packed) {
    return unpackSnorm2x16(packed);
}

#endif