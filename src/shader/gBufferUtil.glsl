uint packAlbedo(vec3 albedo) {
    return packUnorm4x8(vec4(albedo, 1.0));
}

vec3 unpackAlbedo(uint packed) {
    return unpackUnorm4x8(packed).rgb;
}

uvec2 packNormal(vec3 n) {
    vec2 p = vec2(n) * (1.0 / (abs(n.x) + abs(n.y) + n.z));
    return floatBitsToUint(vec2(p.x + p.y, p.x - p.y));
}

vec3 unpackNormal(uvec2 packed) {
    vec2 p = uintBitsToFloat(packed);
    vec2 temp = vec2(p.x + p.y, p.x - p.y) * 0.5;
    vec3 v = vec3(temp, 1.0 - abs(temp.x) - abs(temp.y));
    return normalize(v);
}

void packGBuffer(
    out uvec4 GBufferA, out uvec4 GBufferB,
    vec3 albedo, vec3 normal, float depth,
    vec2 motion, int materialIdx
) {
    GBufferA.x = packAlbedo(albedo);
    GBufferA.yz = packNormal(normal);
    GBufferA.w = floatBitsToUint(depth);

    GBufferB.xy = floatBitsToUint(motion);
    GBufferB.z = uint(materialIdx);
}